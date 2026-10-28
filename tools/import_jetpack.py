#!/usr/bin/env python3
"""Build private Jetpack pattern data from a user-provided APK or APKM."""
from __future__ import annotations

import argparse
import contextlib
from decimal import Decimal, InvalidOperation
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import struct
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / '.local' / 'jetpack' / 'imported'
MAX_PACKAGE = 512 * 1024 * 1024
MAX_NESTED = 224 * 1024 * 1024
MAX_TEXT = 256 * 1024
MAX_ENTRIES = 30000
MAX_PATTERN = 512
MAX_PATTERN_POINTS = 512
MAX_POINTS = 65535
MAX_PROFILE_WEIGHTS = 4096
MAX_ROCKET_PATTERNS = 32
MAX_ROCKET_MISSILES = 8
ROCKET_CONFIG = 'xml/Streaks/PowerupSlowMotionMissiles.xml'
BASE_CONFIG = 'Configs/coins_patterns/CoinPatterns_1.json'
PROFILE_CONFIGS = ((0, 'Default', BASE_CONFIG),
                   (1, 'Teleporter', 'Configs/coins_patterns/Streaks_CFTCoins_1.json'),
                   (2, 'SAM', 'Configs/coins_patterns/Streaks_SAMCoins_1.json'),
                   (3, 'CoinStreak', 'Configs/coins_patterns/Streaks_CoinStreak_1.json'),
                   (4, 'Gravity', 'Configs/coins_patterns/Streaks_GRAVCoins_1.json'),
                   (5, 'Mech', 'Configs/coins_patterns/Streaks_MECHCoins_1.json'))
OBSTACLE_CONFIGS = ((0, 'xml/ObstaclePatterns.xml'), (2, 'xml/Streaks/GRAVObstacles.xml'),
                    (3, 'xml/Streaks/HOGObstacles.xml'), (4, 'xml/Streaks/MECHObstacles.xml'),
                    (7, 'xml/Streaks/DRAGObstacles.xml'))
RUNNER_SOURCES = ('textures/Characters/Jetpacks/jetpackDefault.tex',
                  'textures/Characters/Barry/defaultBody.tex',
                  'textures/Characters/Barry/defaultHead.tex')


class ImportError(ValueError):
    """The supplied package cannot produce a safe private import."""


def safe_name(name):
    if (not name or len(name) > 1024 or '\\' in name or any(ord(char) < 32 for char in name) or name.startswith('/')
            or ':' in name or any(part in ('.', '..') for part in name.split('/'))):
        raise ImportError('Unsafe archive path')
    return name


def index_archive(archive):
    entries = archive.infolist()
    if len(entries) > MAX_ENTRIES:
        raise ImportError('Archive has too many entries')
    result = {}
    total = 0
    for entry in entries:
        safe_name(entry.filename)
        if entry.filename in result:
            raise ImportError('Duplicate archive path')
        if entry.flag_bits & 1:
            raise ImportError('Encrypted archive entries are unsupported')
        total += entry.file_size
        if total > 2 * 1024 * 1024 * 1024:
            raise ImportError('Archive expanded size exceeds the limit')
        result[entry.filename] = entry
    return result


def read_member(archive, entries, name, limit):
    if name not in entries:
        raise ImportError('Required package data is missing: ' + name)
    entry = entries[name]
    if entry.file_size > limit or entry.file_size > max(1, entry.compress_size) * 200:
        raise ImportError('Archive member exceeds its size or compression limit')
    with archive.open(entry) as stream:
        data = stream.read(limit + 1)
    if len(data) > limit or len(data) != entry.file_size:
        raise ImportError('Archive member size is invalid')
    return data


@contextlib.contextmanager
def game_archive(source):
    with contextlib.ExitStack() as stack:
        archive = stack.enter_context(zipfile.ZipFile(source))
        entries = index_archive(archive)
        containers = []
        if 'split_InstallPack.apk' in entries:
            data = read_member(archive, entries, 'split_InstallPack.apk', MAX_NESTED)
            containers.append({'path': 'split_InstallPack.apk', 'sha256': hashlib.sha256(data).hexdigest()})
            archive = stack.enter_context(zipfile.ZipFile(io.BytesIO(data)))
            entries = index_archive(archive)
        if 'assets/assets.zip' in entries:
            data = read_member(archive, entries, 'assets/assets.zip', MAX_NESTED)
            containers.append({'path': 'assets/assets.zip', 'sha256': hashlib.sha256(data).hexdigest()})
            archive = stack.enter_context(zipfile.ZipFile(io.BytesIO(data)))
            entries = index_archive(archive)
        prefix = '' if BASE_CONFIG in entries else 'assets/'
        if prefix + BASE_CONFIG not in entries:
            raise ImportError('No supported Jetpack game asset archive was found')
        yield archive, entries, prefix, containers


def parse_json(data):
    try:
        return json.loads(data.decode('utf-8-sig'), parse_constant=lambda value: (_ for _ in ()).throw(ImportError('Non-finite JSON value')))
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise ImportError('Invalid game JSON') from error


class BinaryXML:
    def __init__(self, data):
        if len(data) < 16 or data[:6] != b'bxml\x01\x00':
            raise ImportError('Unsupported binary XML header')
        size = struct.unpack_from('<I', data, 8)[0]
        if not size or size > MAX_TEXT or 12 + size + 4 > len(data):
            raise ImportError('Invalid binary XML string table')
        self.data, self.strings, self.position, self.nodes = data, data[12:12 + size], 12 + size, 0

    def integer(self):
        if self.position + 4 > len(self.data):
            raise ImportError('Truncated binary XML')
        value = struct.unpack_from('<I', self.data, self.position)[0]
        self.position += 4
        return value

    def string(self):
        offset = self.integer()
        end = self.strings.find(b'\x00', offset)
        if offset >= len(self.strings) or end < 0 or end - offset > 4096:
            raise ImportError('Invalid binary XML string offset')
        try:
            return self.strings[offset:end].decode('utf-8')
        except UnicodeError as error:
            raise ImportError('Invalid binary XML text') from error

    def node(self, depth=0):
        self.nodes += 1
        if depth > 48 or self.nodes > 20000 or self.position >= len(self.data):
            raise ImportError('Binary XML nesting or node limit exceeded')
        kind = self.data[self.position]
        self.position += 1
        if kind == 4:
            return {'tag': '#text', 'text': self.string()}
        if kind != 1:
            raise ImportError('Unsupported binary XML node')
        tag, attrs = self.string(), {}
        count = self.integer()
        if count > 256:
            raise ImportError('Binary XML attribute limit exceeded')
        for _ in range(count):
            key, value = self.string(), self.string()
            if key in attrs:
                raise ImportError('Duplicate binary XML attribute')
            attrs[key] = value
        count = self.integer()
        if count > 20000:
            raise ImportError('Binary XML child limit exceeded')
        return {'tag': tag, 'attrs': attrs, 'children': [self.node(depth + 1) for _ in range(count)]}

    def decode(self):
        count = self.integer()
        if not 0 < count <= 64:
            raise ImportError('Invalid binary XML root count')
        roots = [self.node() for _ in range(count)]
        if self.position != len(self.data):
            raise ImportError('Trailing binary XML data')
        return roots


def pattern_points(data):
    try:
        rows = data.decode('ascii').splitlines()
    except UnicodeError as error:
        raise ImportError('Coin pattern must be ASCII') from error
    width = max(map(len, rows), default=0)
    if not 0 < len(rows) <= 40 or not 0 < width <= 256:
        raise ImportError('Coin pattern dimensions must fit 256 columns and 40 rows; taller layouts need a new renderer')
    if any(set(row) - set('01 ') for row in rows):
        raise ImportError('Coin pattern contains unsupported cells')
    points = sorted(((y << 8) | x for y, row in enumerate(rows) for x, cell in enumerate(row) if cell == '1'),
                    key=lambda point: (point & 255, point >> 8))
    if len(points) > MAX_PATTERN_POINTS:
        raise ImportError('Coin pattern exceeds 512 occupied cells')
    return width, len(rows), points


def base_weights(config):
    try:
        patterns = config['coin_pattern_file']['coin_pattern']
        if not isinstance(patterns, list) or len(patterns) > MAX_PATTERN:
            raise ImportError('Invalid base coin pattern list')
        result = {}
        for pattern in patterns:
            name = safe_name(pattern['filename'])
            probability = pattern['probability']
            if isinstance(probability, list):
                probability = probability[0]
            weight = Decimal(str(probability['value']))
            if not weight.is_finite() or not 0 <= weight <= 655:
                raise ImportError('Invalid coin pattern weight')
            if weight and not probability.get('leveltag'):
                result[name] = int(weight * 100)
        return result
    except (KeyError, TypeError, InvalidOperation, IndexError) as error:
        raise ImportError('Invalid base coin pattern config') from error


def profile_weights(config, profile):
    try:
        patterns = config['coin_pattern_file']['coin_pattern']
        if not isinstance(patterns, list) or not 0 < len(patterns) <= MAX_PATTERN:
            raise ImportError('Invalid profile coin pattern list')
        result, names = [], set()
        for pattern in patterns:
            name = safe_name(pattern['filename'])
            if name in names:
                raise ImportError('Duplicate profile coin pattern')
            names.add(name)
            probabilities = pattern['probability']
            if not isinstance(probabilities, list):
                probabilities = [probabilities]
            if not 0 < len(probabilities) <= 256:
                raise ImportError('Invalid profile probability list')
            starts = set()
            for probability in probabilities:
                if probability.get('leveltag'):
                    continue
                weight = Decimal(str(probability['value']))
                start = Decimal(str(probability.get('startDistance', 0)))
                if not weight.is_finite() or not 0 <= weight <= 655:
                    raise ImportError('Invalid profile coin weight')
                if not start.is_finite() or not 0 <= start <= 65535 or start != int(start):
                    raise ImportError('Invalid profile start distance')
                if int(start) in starts:
                    raise ImportError('Duplicate profile start distance')
                starts.add(int(start))
                result.append({'name': name, 'startMeters': int(start),
                               'weight': int(weight * 100), 'profile': profile})
                if len(result) > MAX_PROFILE_WEIGHTS:
                    raise ImportError('Profile coin weight count exceeds the limit')
        return result
    except (KeyError, TypeError, AttributeError, InvalidOperation) as error:
        raise ImportError('Invalid profile coin pattern config') from error


def runner_frames(read):
    sheets, evidence = [], []
    for name in RUNNER_SOURCES:
        data = read(name, 3 * 1024 * 1024)
        if len(data) < 52 or data[:4] != b'TEX\x01' or data[7:15] != bytes((1, 4, 2, 4, 3, 4, 4, 4)):
            raise ImportError('Unsupported runner TEX format')
        width, height = struct.unpack_from('<II', data, 28)
        if not 128 <= width <= 1024 or not 64 <= height <= 1024 or len(data) != 52 + width * height * 2:
            raise ImportError('Invalid runner texture dimensions')
        values = struct.unpack_from('<' + str(width * height) + 'H', data, 52)
        sheets.append((width, values))
        evidence.append({'path': name, 'sha256': hashlib.sha256(data).hexdigest(), 'width': width, 'height': height})
    composites = []
    for frame in range(6):
        pixels = [None] * (64 * 64)
        for (width, values), (ox, oy) in zip(sheets, ((-12, 0), (0, -3), (0, 9))):
            for y in range(32):
                for x in range(32):
                    value = values[(frame // 4 * 32 + y) * width + frame % 4 * 32 + x]
                    if value & 15 < 8:
                        continue
                    dest_x, dest_y = 16 + ox + x, 16 - oy + y
                    rgb = ((value >> 12) * 17, (value >> 8 & 15) * 17, (value >> 4 & 15) * 17)
                    color = (rgb[0] >> 3 << 11) | (rgb[1] >> 2 << 5) | (rgb[2] >> 3)
                    pixels[dest_y * 64 + dest_x] = color if color != 0xf81f else 0xf01f
        composites.append(pixels)
    occupied = [(i % 64, i // 64) for pixels in composites for i, color in enumerate(pixels) if color is not None]
    if not occupied:
        raise ImportError('Runner textures contain no visible pixels')
    left, right = min(x for x, y in occupied), max(x for x, y in occupied) + 1
    top, bottom = min(y for x, y in occupied), max(y for x, y in occupied) + 1
    width, height = right - left, bottom - top
    scale = min(20 / width, 15 / height)
    fitted_w, fitted_h = max(1, int(width * scale)), max(1, int(height * scale))
    frames = []
    for pixels in composites:
        fitted = [0xf81f] * 300
        for y in range(fitted_h):
            for x in range(fitted_w):
                value = pixels[(top + y * height // fitted_h) * 64 + left + x * width // fitted_w]
                fitted[(y + (15 - fitted_h) // 2) * 20 + x + (20 - fitted_w) // 2] = 0xf81f if value is None else value
        frames.append(fitted)
    return frames, {'sources': evidence, 'method': 'Layer offsets from package; 32x32 cells and run0..3/flight4..5 pose mapping are approximations. Nearest sampling fits a shared extent to20x15. Crash poses use authored fallback.',
                    'source_bounds': [left, top, right, bottom]}


def rocket_waves(roots):
    def number(attrs, key, lower, upper, default=None, integral=False):
        value = Decimal(str(attrs[key] if key in attrs else default))
        if not value.is_finite() or not lower <= value <= upper or integral and value != int(value):
            raise ImportError('Invalid Rocket Time ' + key)
        return int(value) if integral else value

    def boolean(attrs, key, default='false'):
        value = attrs.get(key, default)
        if value not in ('true', 'false'):
            raise ImportError('Invalid Rocket Time ' + key)
        return value == 'true'

    def child(node, tag):
        nodes = [item for item in node.get('children', []) if item.get('tag') == tag]
        if len(nodes) != 1:
            raise ImportError('Invalid Rocket Time ' + tag)
        return nodes[0].get('attrs', {})

    try:
        if len(roots) != 1 or roots[0].get('tag') != 'missile_file':
            raise ImportError('Invalid Rocket Time root')
        root = roots[0]
        speed = number(child(root, 'missileSpeed'), 'value', 0, 100)
        chase = number(child(root, 'chaseSpeed'), 'value', 0, 100)
        nodes = [node for node in root.get('children', []) if node.get('tag') == 'missile_pattern']
        if not 0 < len(nodes) <= MAX_ROCKET_PATTERNS:
            raise ImportError('Rocket Time pattern count exceeds the limit')
        patterns, missiles = [], []
        for node in nodes:
            probability = child(node, 'probability')
            weight = number(probability, 'value', 0, 655)
            if number(probability, 'startDistance', 0, 65535, 0, True) != 0 or probability.get('leveltag'):
                raise ImportError('Unsupported Rocket Time pattern probability')
            actors = [item for item in node.get('children', []) if item.get('tag') == 'missile']
            if not 0 < len(actors) <= MAX_ROCKET_MISSILES:
                raise ImportError('Rocket Time missile count exceeds the limit')
            patterns.append({'offset': len(missiles), 'count': len(actors), 'weight': int(weight * 100)})
            for actor in actors:
                attrs = actor.get('attrs', {})
                delay = number(attrs, 'delay', -120, 3600, integral=True)
                if number(attrs, 'scale', 1, 1, 1) != 1:
                    raise ImportError('Unsupported Rocket Time missile scale')
                position, flight = child(actor, 'position'), child(actor, 'flight')
                random_lane, ranged = boolean(position, 'random'), boolean(position, 'range')
                low = number(position, 'lower_y', -240, 240) if ranged else number(position, 'y', -240, 240, 0)
                high = number(position, 'upper_y', -240, 240) if ranged else low
                low, high = min(low, high), max(low, high)
                angle = number(flight, 'ricochet_angle', -90, 90, 0)
                missile_speed = number(flight, 'speed', 0, 100, 1)
                flags = int(random_lane) | (int(ranged) << 1) | (int(boolean(flight, 'ricochet')) << 2) | (int(boolean(flight, 'dud')) << 3)
                missiles.append({'launchTicks': 150 + delay, 'warningTicks': 120 + delay,
                                 'laneMinQ8': int(low * 256 / 120), 'laneMaxQ8': int(high * 256 / 120),
                                 'angleQ8': int(angle * 256), 'speedQ8': int(missile_speed * 256),
                                 'direction': -1, 'flags': flags})
        if not any(pattern['weight'] for pattern in patterns):
            raise ImportError('Rocket Time patterns have no weight')
        return {'patterns': patterns, 'missiles': missiles,
                'missileSpeedQ8': int(speed * 256), 'chaseSpeedQ8': int(chase * 256)}
    except (KeyError, TypeError, AttributeError, InvalidOperation) as error:
        raise ImportError('Invalid Rocket Time pattern config') from error


def placement_decimal(raw):
    try:
        return Decimal(str(raw))
    except (InvalidOperation, ValueError, TypeError) as error:
        raise ImportError('Invalid numeric placement field') from error


def placement_weights(node):
    result = []
    for key in ('TopWeight', 'BottomWeight', 'TopHalfWeight', 'BottomHalfWeight'):
        raw = node.get(key, {}).get('value', 0)
        value = placement_decimal(raw)
        if not value.is_finite() or not 0 <= value <= 100 or value != int(value):
            raise ImportError('Placement percentages must be integers from 0 to 100')
        result.append(int(value))
    return result


def obstacle_records(roots, profile):
    if len(roots) != 1 or roots[0].get('tag') != 'obstacle_pattern_file':
        raise ImportError('Invalid obstacle config root')
    children = roots[0].get('children', [])
    weights = placement_weights({child['tag']: child.get('attrs', {}) for child in children if child['tag'] != 'obstacle_pattern'})
    records = []
    template_id = 0
    for node in children:
        if node['tag'] != 'obstacle_pattern':
            continue
        values = {child['tag']: child.get('attrs', {}) for child in node.get('children', []) if child['tag'] != 'probability'}
        def number(key, default, low, high):
            value = placement_decimal(values.get(key, {}).get('value', default))
            if not value.is_finite() or value != int(value) or not low <= value <= high:
                raise ImportError('Invalid obstacle field: ' + key)
            return int(value)
        minimum = number('minScale', 32, 1, 512)
        maximum = number('maxScale', minimum, minimum, 512)
        angle = number('startAngle', 0, 0, 359)
        period = number('rotateTime', 0, 0, 3600)
        clockwise = values.get('clockwise', {}).get('value', 'true')
        allowed = values.get('allowedWithLasers', {}).get('value', 'true')
        if clockwise not in ('true', 'false') or allowed not in ('true', 'false'):
            raise ImportError('Invalid obstacle boolean')
        for probability in node.get('children', []):
            if probability['tag'] != 'probability':
                continue
            attrs = probability.get('attrs', {})
            weight = placement_decimal(attrs.get('value', 0))
            start = placement_decimal(attrs.get('startDistance', 0))
            if not weight.is_finite() or not 0 <= weight <= 655 or not start.is_finite() or start != int(start) or not 0 <= start <= 65535:
                raise ImportError('Invalid obstacle probability')
            records.append(dict(profile=profile, template=template_id, minimum=minimum, maximum=maximum, angle=angle, period=period,
                                flags=int(clockwise == 'true') | (int(allowed == 'true') << 1),
                                start=int(start), weight=int(weight * 100), placement=weights))
        template_id += 1
    if len(records) > 64:
        raise ImportError('Too many obstacle records')
    return records


def header(patterns, points, frames, profile_records=None, rocket=None, placements=None, obstacles=None):
    lines = ['#pragma once', '#include <stdint.h>', '// Private imported data. Rebuild from your own source package.',
             'namespace flappy { namespace JetpackImported {',
             'struct Pattern { uint16_t offset, count, width, height, weight; uint8_t flags; };',
             'static const uint16_t CoinPoints[] = {']
    for index in range(0, len(points), 16):
        lines.append('  ' + ','.join('0x%04x' % point for point in points[index:index + 16]) + ',')
    if not points:
        lines.append('  0,')
    lines += ['};', 'static const Pattern CoinPatterns[] = {']
    lines += ['  {%d,%d,%d,%d,%d,%d},' % tuple(pattern[k] for k in ('offset', 'count', 'width', 'height', 'weight', 'flags')) for pattern in patterns]
    lines += ['};', 'static const unsigned PatternCount = %d;' % len(patterns),
              'static const unsigned CoinPointCount = %d;' % len(points),
              'static const unsigned MaxPatternPoints = %d;' % max(pattern['count'] for pattern in patterns)]
    if profile_records is not None:
        lines[2:2] = ['#define M5FLAP_JETPACK_PROFILE_WEIGHTS 1']
        lines += ['enum ProfileId { ProfileDefault=0, ProfileTeleporter=1, ProfileSAM=2,',
                  '                 ProfileCoinStreak=3, ProfileGravity=4, ProfileMech=5 };',
                  'struct ProfileWeight { uint16_t pattern, startMeters, weight; uint8_t profile; };',
                  'static const ProfileWeight ProfileWeights[] = {']
        lines += ['  {%d,%d,%d,%d},' % tuple(record[key] for key in ('pattern', 'startMeters', 'weight', 'profile'))
                  for record in profile_records]
        if not profile_records:
            lines.append('  {0,0,0,0},')
        lines += ['};', 'static const unsigned ProfileWeightCount = %d;' % len(profile_records)]
    if rocket is not None:
        lines[2:2] = ['#define M5FLAP_JETPACK_ROCKET_WAVES 1']
        lines += ['struct RocketMissile { uint16_t launchTicks, warningTicks; int16_t laneMinQ8, laneMaxQ8, angleQ8; uint16_t speedQ8; int8_t direction; uint8_t flags; };',
                  'struct RocketPattern { uint16_t offset, weight; uint8_t count; };',
                  'static const RocketMissile RocketMissiles[] = {']
        lines += ['  {%d,%d,%d,%d,%d,%d,%d,%d},' % tuple(missile[key] for key in
                  ('launchTicks', 'warningTicks', 'laneMinQ8', 'laneMaxQ8', 'angleQ8', 'speedQ8', 'direction', 'flags'))
                  for missile in rocket['missiles']]
        lines += ['};', 'static const RocketPattern RocketPatterns[] = {']
        lines += ['  {%d,%d,%d},' % tuple(pattern[key] for key in ('offset', 'weight', 'count'))
                  for pattern in rocket['patterns']]
        lines += ['};', 'static const unsigned RocketPatternCount = %d;' % len(rocket['patterns']),
                  'static const unsigned RocketMissileCount = %d;' % len(rocket['missiles']),
                  'static const uint16_t RocketMissileSpeedQ8 = %d;' % rocket['missileSpeedQ8'],
                  'static const uint16_t RocketChaseSpeedQ8 = %d;' % rocket['chaseSpeedQ8']]
    if placements is not None:
        lines[2:2] = ['#define M5FLAP_JETPACK_PLACEMENT 1']
        lines += ['struct CoinPlacement { uint8_t profile, top, bottom, topHalf, bottomHalf; };',
                  'static const CoinPlacement CoinPlacements[] = {']
        lines += ['  {%d,%d,%d,%d,%d},' % tuple(record) for record in placements]
        lines += ['};', 'static const unsigned CoinPlacementCount = %d;' % len(placements)]
    if obstacles:
        lines[2:2] = ['#define M5FLAP_JETPACK_OBSTACLES 1']
        lines += ['struct Obstacle { uint16_t minimum, maximum, angle, period, start, weight; uint8_t profile, flags, templateId, top, bottom, topHalf, bottomHalf; };',
                  'static const Obstacle Obstacles[] = {']
        lines += ['  {%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d},' %
                  (tuple(record[key] for key in ('minimum','maximum','angle','period','start','weight','profile','flags','template')) + tuple(record['placement']))
                  for record in obstacles]
        lines += ['};', 'static const unsigned ObstacleCount = %d;' % len(obstacles)]
    lines += ['static const bool HasRunner = %s;' % ('true' if frames else 'false'),
              'static const unsigned FrameCount = %d;' % len(frames),
              'static const uint16_t RunnerPixels[][300] = {']
    for frame in frames:
        lines.append('  {' + ','.join('0x%04x' % pixel for pixel in frame) + '},')
    if not frames:
        lines.append('  {0xf81f},')
    return '\n'.join(lines + ['};', '} }', ''])


def import_package(source, output=DEFAULT_OUTPUT, *, accept_rights=False, original_runner=False):
    if not accept_rights:
        raise ImportError('Confirm your rights with --accept-rights before importing')
    source, output = Path(source).resolve(), Path(output).resolve()
    if output == ROOT or ROOT in output.parents and ROOT / '.local' not in output.parents:
        raise ImportError('Output inside the repository must be under .local')
    if not source.is_file() or source.stat().st_size > MAX_PACKAGE:
        raise ImportError('Source package is missing or exceeds 512 MiB')
    digest = hashlib.sha256()
    with source.open('rb') as stream:
        for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            digest.update(chunk)
    evidence, patterns, points, configs, rocket = [], [], [], {}, None
    placements, obstacles = [], []
    try:
        with game_archive(source) as (archive, entries, prefix, containers):
            def read(name, limit=MAX_TEXT):
                data = read_member(archive, entries, prefix + name, limit)
                evidence.append({'path': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
                return data
            base_config = parse_json(read(BASE_CONFIG))
            placements.append([0] + placement_weights(base_config['coin_pattern_file']))
            weights = base_weights(base_config)
            profile_records = profile_weights(base_config, 0)
            profile_configs = [0]
            for profile, label, name in PROFILE_CONFIGS[1:]:
                if prefix + name in entries:
                    config = parse_json(read(name))
                    configs[name] = config
                    placements.append([profile] + placement_weights(config['coin_pattern_file']))
                    profile_records.extend(profile_weights(config, profile))
                    profile_configs.append(profile)
                    if len(profile_records) > MAX_PROFILE_WEIGHTS:
                        raise ImportError('Combined profile coin weight count exceeds the limit')
            names = sorted(name[len(prefix):] for name in entries if name.startswith(prefix + 'patterns/') and name.endswith('.txt'))
            if not 0 < len(names) <= MAX_PATTERN:
                raise ImportError('Pattern count exceeds the supported limit')
            missing_base_patterns = sorted(set(weights) - set(names))
            indices = {name: index for index, name in enumerate(names)}
            missing_profile_patterns = {label: sorted({record['name'] for record in profile_records
                                                       if record['profile'] == profile and record['name'] not in indices})
                                        for profile, label, _ in PROFILE_CONFIGS if profile in profile_configs}
            memberships = {}
            imported_profile_records = []
            for record in profile_records:
                if record['name'] not in indices:
                    continue
                if record['profile'] and record['weight']:
                    memberships[record['name']] = memberships.get(record['name'], 0) | (1 << (record['profile'] + 2))
                imported_profile_records.append({key: record[key] for key in ('startMeters', 'weight', 'profile')}
                                               | {'pattern': indices[record['name']]})
            imported_profile_records.sort(key=lambda record: (record['profile'], record['pattern'], record['startMeters']))
            for name in names:
                width, height, coords = pattern_points(read(name, 65536))
                if len(points) + len(coords) > MAX_POINTS:
                    raise ImportError('Combined coin point count exceeds 65535')
                flags = int(name in weights) | (2 if PurePosixPath(name).name.startswith('cashDash') else 0) | (4 if 'onboarding' in name.lower() else 0)
                flags |= memberships.get(name, 0)
                patterns.append({'name': name, 'offset': len(points), 'count': len(coords), 'width': width, 'height': height, 'weight': weights.get(name, 0), 'flags': flags})
                points.extend(coords)
            if not any(pattern['flags'] & 1 and pattern['count'] for pattern in patterns):
                raise ImportError('No nonempty base classic coin pattern was found')
            for name in ('xml/settings.xml', 'xml/LevelPacing.xml', 'xml/ObstaclePatterns.xml', 'xml/MissileProbability.xml', 'xml/vehicleProbability.xml'):
                if prefix + name in entries:
                    configs[name] = BinaryXML(read(name)).decode()
            for profile, name in OBSTACLE_CONFIGS:
                if prefix + name in entries:
                    if name not in configs:
                        configs[name] = BinaryXML(read(name)).decode()
                    obstacles.extend(obstacle_records(configs[name], profile))
            if prefix + ROCKET_CONFIG in entries:
                configs[ROCKET_CONFIG] = BinaryXML(read(ROCKET_CONFIG)).decode()
                rocket = rocket_waves(configs[ROCKET_CONFIG])
            json_names = ('Configs/game_mode_rules/classic_1.json',) + tuple('Configs/vehicles/' + name + '_1.json' for name in ('GravitySuit', 'Hog', 'mech', 'ProfitBird', 'Teleporter', 'Dragon', 'dragonInfo'))
            for name in json_names:
                if prefix + name in entries:
                    configs[name] = parse_json(read(name))
            frames, art = runner_frames(read) if original_runner else ([], None)
    except (zipfile.BadZipFile, OSError, RuntimeError, struct.error, RecursionError) as error:
        raise ImportError('Invalid or unreadable source archive') from error
    generated = header(patterns, points, frames, imported_profile_records, rocket, placements, obstacles)
    receipt = {'format_version': 2, 'source_sha256': digest.hexdigest(), 'source_bytes': source.stat().st_size,
               'rights_acknowledged': True, 'containers': containers, 'patterns': patterns, 'point_count': len(points),
               'base_pattern_count': sum(bool(pattern['flags'] & 1) for pattern in patterns),
               'missing_base_patterns': missing_base_patterns,
               'profile_ids': {label: profile for profile, label, _ in PROFILE_CONFIGS if profile in profile_configs},
               'profile_weight_count': len(imported_profile_records),
               'missing_profile_patterns': missing_profile_patterns,
               'rocket_pattern_count': len(rocket['patterns']) if rocket else 0,
               'rocket_missile_count': len(rocket['missiles']) if rocket else 0,
               'runner': art, 'files': evidence, 'header_sha256': hashlib.sha256(generated.encode()).hexdigest()}
    outputs = {'jetpack_imported.h': generated, 'receipt.json': json.dumps(receipt, indent=2, sort_keys=True) + '\n',
               'reference-config.json': json.dumps(configs, indent=2, sort_keys=True) + '\n'}
    output.mkdir(parents=True, exist_ok=True)
    for name, data in outputs.items():
        with tempfile.NamedTemporaryFile('w', encoding='utf-8', dir=output, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        os.replace(temporary, output / name)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument('--accept-rights', action='store_true', help='I have the rights needed to use and transform this package on my devices')
    parser.add_argument('--original-runner', action='store_true', help='Privately fit original runner layers to20x15; animation mapping is approximate')
    args = parser.parse_args()
    try:
        receipt = import_package(args.source, args.output, accept_rights=args.accept_rights, original_runner=args.original_runner)
    except ImportError as error:
        parser.exit(2, 'Import failed: ' + str(error) + '\n')
    print('Imported %d patterns and %d coin points into %s' % (len(receipt['patterns']), receipt['point_count'], args.output))
    if receipt['missing_base_patterns']:
        print('The receipt lists %d base config references missing from this package.' % len(receipt['missing_base_patterns']))


if __name__ == '__main__':
    main()
