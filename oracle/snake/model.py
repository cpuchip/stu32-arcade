"""Snake's rules from docs/snake.md alone: the grid, the turn queue, the step and the food.

Nothing here reads the C. tests/snake_trace.c prints the same lines, and oracle/snake_check.sh compares them.
Script lines:
  game SEED N           a new game: seed (as app_seed() << 32 gives it) and the game number
  case DIR FX,FY X,Y ... a hand-made state: the direction (0 up, 1 right, 2 down, 3 left), the food, then the body
                        from the tail to the head; the food state then draws as game 0 of seed 0
  key K                 a direction key's down (8 up, 6 right, 2 down, 4 left)
  turn L|R              TURN mode: a quarter turn left or right of the last queued direction
  step N                N steps (they stop at the end)
  interval E            the step interval after E foods
  end?                  'flying' when the game has not ended
  head?                 the head, the direction and the length
"""
import sys
from collections import deque

W, H = 40, 22
CELLS = W * H
QUEUE = 2
DX = (0, 1, 0, -1)
DY = (-1, 0, 1, 0)
KEYS = {'8': 0, '6': 1, '2': 2, '4': 3}
M64 = (1 << 64) - 1


def mix64(z):
    z = (z + 0x9E3779B97F4A7C15) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def interval(eaten):
    return max(60, 150 - 5 * eaten)


class Snake:
    def __init__(self, seed, game, body=None, direction=1):
        self.x = mix64((seed ^ mix64(game)) & M64)
        self.body = deque(body if body else [10 * W + 7, 10 * W + 8, 10 * W + 9, 10 * W + 10])  # tail .. head
        self.occ = set(self.body)
        self.dir = direction
        self.queue = []
        self.eaten = 0
        self.end = None
        self.food = -1

    def place_food(self):
        free = CELLS - len(self.body)
        if free == 0:
            self.food = -1
            return False
        self.x = mix64(self.x)
        k = self.x % free
        for c in range(CELLS):
            if c not in self.occ:
                if k == 0:
                    self.food = c
                    return True
                k -= 1
        raise AssertionError

    def offer(self, d):
        last = self.queue[-1] if self.queue else self.dir
        if len(self.queue) < QUEUE and d != last and d != (last + 2) % 4:
            self.queue.append(d)
            return True
        return False

    def turn(self, right):
        """TURN mode: a quarter turn left or right of the last queued direction (or the current one), offered"""
        last = self.queue[-1] if self.queue else self.dir
        return self.offer((last + (1 if right else 3)) % 4)

    def step(self):
        if self.end:
            return self.end
        if self.queue:
            self.dir = self.queue.pop(0)
        head = self.body[-1]
        x, y = head % W + DX[self.dir], head // W + DY[self.dir]
        if not (0 <= x < W and 0 <= y < H):
            self.end = 'dead'
            return self.end
        nxt = y * W + x
        if nxt == self.food:
            self.body.append(nxt)
            self.occ.add(nxt)
            self.eaten += 1
            if not self.place_food():
                self.end = 'won'
            return self.end
        if nxt in self.occ and nxt != self.body[0]:   # the tail (body[0]) is freed first
            self.end = 'dead'
            return self.end
        self.occ.discard(self.body.popleft())
        self.body.append(nxt)
        self.occ.add(nxt)
        return None

    def line(self, t):
        h = self.body[-1]
        ck = 0
        for i, c in enumerate(self.body):
            ck = (ck + c * (i + 1)) & 0xFFFFFFFF
        q = ''.join(str(d) for d in self.queue) or '-'
        return (f"{t} head={h % W},{h // W} len={len(self.body)} dir={self.dir} queue={q} food={self.food} "
                f"eaten={self.eaten} body={ck}")


def run(lines):
    out = []
    s = None
    t = 0
    for line in lines:
        w = line.split()
        if not w or w[0].startswith('#'):
            continue
        if w[0] == 'game':
            s = Snake(int(w[1]), int(w[2]))
            s.place_food()
            t = 0
            out.append(s.line(t))
        elif w[0] == 'case':
            fx, fy = map(int, w[2].split(','))
            body = [int(p.split(',')[1]) * W + int(p.split(',')[0]) for p in w[3:]]
            s = Snake(0, 0, body, int(w[1]))
            s.food = fy * W + fx
            t = 0
        elif w[0] == 'key':
            taken = s.offer(KEYS[w[1]])
            out.append(f"key {w[1]} {'taken' if taken else 'dropped'} queue={''.join(str(d) for d in s.queue) or '-'}")
        elif w[0] == 'turn':
            taken = s.turn(w[1] == 'R')
            out.append(f"turn {w[1]} {'taken' if taken else 'dropped'} queue={''.join(str(d) for d in s.queue) or '-'}")
        elif w[0] == 'step':
            for _ in range(int(w[1])):
                if s.end:
                    break
                r = s.step()
                t += 1
                out.append(s.line(t))
                if r:
                    out.append(f'end {r}')
        elif w[0] == 'end?':
            if not s.end:
                out.append('flying')
        elif w[0] == 'head?':
            h = s.body[-1]
            out.append(f'head {h % W},{h // W} dir={s.dir} len={len(s.body)}')
        elif w[0] == 'interval':
            out.append(f'interval {w[1]} {interval(int(w[1]))}')
    return out


if __name__ == '__main__':
    with open(sys.argv[1]) as f:
        print('\n'.join(run(f.read().splitlines())))
