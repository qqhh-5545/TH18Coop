#include <assert.h>
#include <stdio.h>
#include "../src/th14/items14.h"

int main(void) {
    ItemSeat14 seats[3] = {
        {1, 0, -100, 100, -105, 95, -95, 105},
        {1, 1, 100, 380, 95, 375, 105, 385},
        {1, 1, 110, 380, 105, 375, 115, 385}
    };
    ItemRoute14 route = item14_route(100, 340, 3, ITEM14_FALLING, -1, -1, 0, 0, seats);
    assert(route.owner == 0 && route.attract); // Blue point follows the trigger.
    route = item14_route(100, 340, 3, ITEM14_FALLING, -1, -1, 0, 1, seats);
    assert(route.owner == 1 && route.attract); // P can reach a player below the line.
    route = item14_route(-100, 100, 3, ITEM14_FALLING, -1, -1, -1, 1, seats);
    assert(route.owner == 0 && !route.attract); // Native contact confirms the full-power pickup before redirect.
    seats[0].y=380;seats[0].x=101;
    seats[0].left=100.5f;seats[0].right=102;seats[0].top=379;seats[0].bottom=381;
    seats[1].x=103;seats[1].left=98;seats[1].right=108;
    route = item14_route(100, 380, 3, ITEM14_FALLING, -1, -1, -1, 1, seats);
    assert(route.owner==1 && !route.attract); // Closest center misses, eligible teammate touches.
    seats[0]=(ItemSeat14){1,0,-100,100,-105,95,-95,105};seats[1].x=100;
    route = item14_route(109, 380, 3, ITEM14_HOMING, 1, -1, -1, 1, seats);
    assert(route.owner == 1 && !route.attract);
    seats[1].needs_power = 0;
    route = item14_route(105, 380, 3, ITEM14_HOMING, 1, -1, -1, 1, seats);
    assert(route.owner == 2);
    route = item14_route(109, 380, 3, ITEM14_NEARBY, 1, 1, 0, 1, seats);
    assert(route.owner == 1 && !route.attract); // A gift keeps its designated recipient.
    seats[1].live = 0;
    route = item14_route(100, 380, 3, ITEM14_HOMING, 1, 1, -1, 1, seats);
    assert(route.owner == 2);
    seats[2].needs_power = 0;
    route = item14_route(105, 380, 3, ITEM14_FALLING, -1, -1, 0, 1, seats);
    assert(route.owner == 2 && route.attract); // Whole team full: nearest wins.
    route = item14_route(110, 380, 3, ITEM14_FALLING, -1, -1, -1, 1, seats);
    assert(route.owner == 2 && !route.attract); // Native full-power scoring is reached normally.
    seats[0].live = seats[2].live = 0;
    route = item14_route(105, 380, 3, ITEM14_FALLING, -1, -1, -1, 1, seats);
    assert(route.owner == ITEM14_NONE);
    seats[0] = (ItemSeat14){.live=1,.x=90,.y=380};
    seats[2].live = 1;
    route = item14_route(100, 380, 3, ITEM14_FALLING, -1, -1, 2, 1, seats);
    assert(route.owner == 0 && route.attract); // Lower seat wins ties.
    puts("PASS TH14 item routing: Bomb/line, below-line and full-power P, stable targets, gifts, ghosts and ties");
}
