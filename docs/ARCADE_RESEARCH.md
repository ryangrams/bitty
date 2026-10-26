# Arcade references and adaptation

Research checked October 26, 2026. The sources below describe two separate arcade games: Bay Tek's licensed Flappy Bird and Adrenaline Amusements' Flying Tickets. Their rules and payout menus differ. This project uses arcade-inspired rules tuned for a small handheld display.

## Bay Tek Flappy Bird

Bay Tek's [current service page](https://parts.baytekent.com/flappy-bird/) links a [manufacturer-hosted service manual](https://a.storyblok.com/f/287084236645570/x/6344821f23/public-flappy-bird-service-manual.pdf). Page references here use the page numbers printed inside each manual.

The manual's page 5 says: "Get tickets for every set of pipes you pass through."

Page 9 exposes eight difficulty levels through vertical gap size, automatic adjustment, and a training setting that keeps the player alive until a chosen early pipe. Page 11 lists these payout defaults:

| Setting | Default |
| --- | --- |
| Pipes with tickets | 10 |
| Ticket value | 1 |
| Bonus coin interval | 5 pipes |
| Bonus coin value | 10 tickets |
| Jackpot value | 250 tickets |
| Jackpot increment | 1 ticket |
| Jackpot location | Pipe 42 |

The payout menu also exposes fixed payouts, training tickets, ticket caps, and a divide-by-two option. These settings prevent a universal score-to-ticket conversion. The description of "pipes with tickets" does not establish a complete scoring formula. [Manufacturer manual, pages 9 and 11](https://a.storyblok.com/f/287084236645570/x/6344821f23/public-flappy-bird-service-manual.pdf).

A [Bay Tek-authored alternate manual hosted by BMI Gaming](https://www.bmigaming.com/Games/Manuals/baktek/flappy-bird-arcade-ticket-redemption-video-game-service-manual-baytek.pdf) has different menus. Its page 9 describes an initial gap that narrows, plus a selectable narrowing start. Page 11 supplies five independently configurable bonus positions:

| Pipe | Bonus ticket value |
| --- | --- |
| 10 | 10 |
| 20 | 20 |
| 35 | 50 |
| 50 | 500 |
| 60 | 1000 |

That menu also has ordinary ticket spacing and value. The documents do not identify enough revision history to match every setting to a specific cabinet software version. The five values are bonus amounts at separate pipe positions. Treating the latest value as the entire ticket total would discard ordinary pipe awards and earlier rewards. Whether a bonus replaces or supplements the ordinary award on its exact pipe remains unspecified. [Alternate manual, pages 9 and 11](https://www.bmigaming.com/Games/Manuals/baktek/flappy-bird-arcade-ticket-redemption-video-game-service-manual-baytek.pdf).

### Manufacturer video observations

I opened and visually inspected selected frames of [Bay Tek Games: Flappy Bird](https://www.youtube.com/watch?v=Rm0nq3f_ekY), published by Bay Tek Entertainment on September 2, 2014. These observations come from the displayed video frames:

| Video time | Visible detail |
| --- | --- |
| [0:51](https://www.youtube.com/watch?v=Rm0nq3f_ekY&t=51s) | A button tutorial fills the playfield. A large ticket counter sits below the grass. |
| [0:56](https://www.youtube.com/watch?v=Rm0nq3f_ekY&t=56s) | A blue pipe gap contains a medal and a large reward number, 10. |
| [1:11](https://www.youtube.com/watch?v=Rm0nq3f_ekY&t=71s) | The bottom ticket counter reads 14. A blue bonus gap advertises 10. |
| [1:21](https://www.youtube.com/watch?v=Rm0nq3f_ekY&t=81s) | The result panel separates score 40, best 40, and tickets 59. |

The manufacturer's video description confirms one-button control, death on pipe or ground contact, ticket awards, and daily/all-time high scores. The edited footage cannot establish a payout formula or precise timing constants. [Official video and description](https://www.youtube.com/watch?v=Rm0nq3f_ekY).

## Adrenaline Flying Tickets

The [manufacturer's product page](https://adrenalinearcade.com/flying-tickets-2/) advertises one-button play and two players. Its support link opens a [public manufacturer manual library](https://drive.google.com/drive/folders/1mzps-mE5JS6CLxvCKxO3utcYVJReJQxj). I opened `FlyingTickets_Manual_65inch_ V4.3.pdf` there and read pages 7 and 16.

Page 7 assigns one ticket to green pipes and five to red/white pipes. Players beat a target for a bonus. Page 16 allows red-pipe frequency from 1 to 50 percent, bonus value from 50 to 1000, and average games per bonus from 10 to 1000, with 250 shown as the default. The gameplay introduction gives a narrower bonus range of 100 to 1000. Use the operator table when describing configurable values. [Official V4.3 manual](https://drive.google.com/file/d/1PyU9BOTMaOZNR42He9qg46Q89CeBj0gS/view).

The product page's Gameplay button links [this 34-second manufacturer video](https://adrenalinearcade.com/wp-content/uploads/2024/06/Flappy.mp4). I inspected it in the browser without saving the video:

| Video time | Visible detail |
| --- | --- |
| 0:18 | Blue and red birds share the scrolling obstacles. Each has its own 9-ticket HUD. The banner asks players to beat 51 for a 500-ticket bonus. |
| 0:25 | A red/white striped pipe is visible. The player totals differ, 18 and 23. |
| 0:32 | The result screen shows separate 34-ticket totals and a daily record of 34. |

These frames support shared obstacles, separate totals, and a large target/reward banner. They do not establish bird-to-bird collision rules, synchronization code, or adaptive bonus algorithms. [Official gameplay video](https://adrenalinearcade.com/wp-content/uploads/2024/06/Flappy.mp4).

## Handheld design decisions

The useful arcade cues are large ticket numbers, visible rewards in upcoming gaps, and a result screen that separates cleared pipes from earned tickets. Original artwork and cabinet media remain outside the public source package.

For a clear handheld scoring rule, award one virtual ticket per cleared pipe and add each reached milestone's bonus once. For example, ten pipes with a ten-ticket bonus produce twenty virtual tickets. This is a deliberate adaptation. The cabinet manuals leave some award-order details unresolved. Virtual tickets have no redemption value.

Keep difficulty settings and records separate from the original mode. A narrowing gap can borrow the alternate manual's idea while using constants tested for this display. Exact gravity, flap impulse, scroll speed, collision masks, input latency, and cabinet frame rate were not established by these sources. Label the result arcade-inspired.

## M5StickC Plus hardware notes

The [official board documentation](https://docs.m5stack.com/en/core/m5stickc_plus) identifies Button A on GPIO37, Button B on GPIO39, BM8563 RTC, and the AXP192 power key. The front and right buttons can provide action and menu. The other side button primarily controls power. [M5Unified 0.2.21](https://github.com/m5stack/M5Unified/blob/0.2.21/src/M5Unified.cpp) can report power-key short presses through `BtnPWR`; holding the key still has power consequences.

The [M5GFX 0.2.28 backlight implementation](https://github.com/m5stack/M5GFX/blob/0.2.28/src/M5GFX.cpp#L489) quantizes nonzero brightness using `(((value >> 1) + 8) / 13) + 5`. Values 1, 52, 104, 156, and 255 select distinct regulator codes 5, 7, 9, 11, and 15. Zero disables the backlight. The lowest visible level needs a physical display check.
