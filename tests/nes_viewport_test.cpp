#include "../firmware/flappy/nes_viewport.h"
#include <assert.h>
#include <iostream>

int main() {
  const int displays[][2] = {{240,135},{160,80},{320,240},{135,240}};
  for (const auto& display : displays) {
    flappy::NesViewport view;
    view.configure(display[0],display[1]);
    assert(view.width > 0 && view.height > 0);
    assert(view.x >= 0 && view.y >= 0);
    assert(view.x + view.width <= display[0]);
    assert(view.y + view.height <= display[1]);
    for (int x=0;x<view.width;++x) assert(view.sourceX(x)>=0 && view.sourceX(x)<256);
    for (int y=0;y<view.height;++y) {
      assert(view.sourceY(y)>=0 && view.sourceY(y)<240);
      assert((y < view.firstRow(120)) == (view.sourceY(y)<120));
    }
    assert(view.firstRow(0)==0 && view.firstRow(240)==view.height);
    for (int source=0;source<240;++source) {
      const int begin=view.firstRow(source), end=view.firstRow(source+1);
      for (int row=begin;row<end;++row) assert(view.sourceY(row)==source);
    }
  }
  std::cout << "NES viewport checks passed.\n";
}
