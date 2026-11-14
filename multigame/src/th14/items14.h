#ifndef TH14_ITEMS14_H
#define TH14_ITEMS14_H
#include <math.h>

enum { ITEM14_NONE = -1, ITEM14_FALLING = 1, ITEM14_RISING = 2,
       ITEM14_HOMING = 3, ITEM14_NEARBY = 4 };

typedef struct {
    int live;
    int needs_power;
    float x, y;
    float left, top, right, bottom;
} ItemSeat14;

typedef struct { int owner, attract; } ItemRoute14;

static int item14_nearest(float x, float y, unsigned count, int needs_power,
                          const ItemSeat14 *seats) {
    int best = ITEM14_NONE;
    float distance = INFINITY;
    for (unsigned i = 0; i < count; ++i) {
        if (!seats[i].live || (needs_power && !seats[i].needs_power)) continue;
        float dx = x - seats[i].x, dy = y - seats[i].y;
        float d = dx * dx + dy * dy;
        if (best == ITEM14_NONE || d < distance) {
            distance = d;
            best = (int)i;
        }
    }
    return best;
}

static int item14_can_receive(int seat, unsigned count, const ItemSeat14 *seats) {
    return seat >= 0 && (unsigned)seat < count && seats[seat].live;
}

static int item14_contains(const ItemSeat14 *seat, float x, float y) {
    return x >= seat->left && x <= seat->right && y >= seat->top && y <= seat->bottom;
}

static int item14_touching(float x, float y, unsigned count, const ItemSeat14 *seats) {
    int best = ITEM14_NONE;
    float distance = INFINITY;
    for (unsigned i = 0; i < count; ++i) {
        if (!seats[i].live || !item14_contains(&seats[i], x, y)) continue;
        float dx = x - seats[i].x, dy = y - seats[i].y;
        float d = dx * dx + dy * dy;
        if (best == ITEM14_NONE || d < distance) {
            distance = d;
            best = (int)i;
        }
    }
    return best;
}

static ItemRoute14 item14_route(float x, float y, unsigned count, unsigned state,
                                int previous, int gift, int trigger, int power,
                                const ItemSeat14 *seats) {
    if (item14_can_receive(gift, count, seats)) return (ItemRoute14){gift, 0};

    if (state == ITEM14_HOMING && item14_can_receive(previous, count, seats) &&
        (!power || seats[previous].needs_power))
        return (ItemRoute14){previous, 0};

    int eligible = power ? item14_nearest(x, y, count, 1, seats) : ITEM14_NONE;
    if (state == ITEM14_HOMING && item14_can_receive(previous, count, seats) && eligible == ITEM14_NONE)
        return (ItemRoute14){previous, 0};

    if (trigger != ITEM14_NONE) {
        int owner = power ? eligible : trigger;
        if (owner == ITEM14_NONE) owner = item14_nearest(x, y, count, 0, seats);
        return (ItemRoute14){owner, 1};
    }

    if (state == ITEM14_HOMING) {
        int owner = eligible != ITEM14_NONE ? eligible : item14_nearest(x, y, count, 0, seats);
        return (ItemRoute14){owner, 0};
    }

    if (state == ITEM14_NEARBY && item14_can_receive(previous, count, seats)) {
        if (power && eligible != ITEM14_NONE && !seats[previous].needs_power)
            return (ItemRoute14){eligible, 1};
        return (ItemRoute14){previous, 0};
    }

    // Choose a seat whose real pickup box overlaps before considering center distance.
    // Native movement can change that contact later in this tick, so the pickup
    // hook separately validates and redirects confirmed full-power contacts.
    int touching = power ? item14_touching(x, y, count, seats) : ITEM14_NONE;
    if (touching != ITEM14_NONE) return (ItemRoute14){touching, 0};
    int closest = item14_nearest(x, y, count, 0, seats);
    return (ItemRoute14){closest, 0};
}
#endif
