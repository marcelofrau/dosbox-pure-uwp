#include <conio.h>
#include <graph.h>

int main() {
    short i;

    _setvideomode(_VRES16COLOR);

    for (i = 0; i < 16; i++) {
        _setcolor(i);
        _rectangle(_GFILLINTERIOR, i * 40, 0, (i + 1) * 40, 200);
    }

    _setcolor(15);
    _rectangle(_GFILLINTERIOR, 200, 60, 440, 180);

    _setcolor(1);
    _rectangle(_GBORDER, 200, 60, 440, 180);

    _settextcolor(14);
    _settextposition(8, 32);
    _outtext("Hello, DOS!");

    _settextcolor(7);
    _settextposition(22, 23);
    _outtext("Press any key to exit...");

    _getch();

    _setvideomode(_DEFAULTMODE);
    return 0;
}
