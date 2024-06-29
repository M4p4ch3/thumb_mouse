
import mouse, sys
import time
from serial import Serial

MS_PER_S = 1000

VAL_TX_DELAY_MS = 10
VAL_TX_DELAY_S = MS_PER_S / VAL_TX_DELAY_MS

VAL_SRC_MIN = 1
VAL_SRC_MAX = 680

VAL_DST_MIN = -1
VAL_DST_MAX = 1
VAL_DST_DEADZONE = 0.1

MOV_FORMULA = "0.9x^5 + 0.1x"

# Pixel/sec
MOV_SPEED_MAX = 4000

def map_val(val_src: int, range_src_min: int, range_src_max: int, range_dst_min: int, range_dst_max: int):
    return (val_src - range_src_min) / range_src_max * (range_dst_max - range_dst_min) + range_dst_min

def apply_formula(val_in: float, formula: str):
    val_out = 0
    for member in formula.replace(" ", "").split("+"):
        coef = float(member.split("x")[0])
        exp = 1
        if "x^" in member:
            exp = int(member.split("x^")[1])
        val_out += coef * pow(val_in, exp)
        print(f" += {coef}x^{exp}")
    print(f"{formula}({val_in}) = {val_out}")
    return val_out

def main():
    # mouse.FAILSAFE = False
    serial = Serial("/dev/ttyACM1", 115200)
    time.sleep(1)

    mouse_mov_x_buf = 0
    mouse_mov_y_buf = 0

    while True:
        data = str(serial.readline().decode('ascii'))
        # print(data)

        val_list = [map_val(int(val_str), VAL_SRC_MIN, VAL_SRC_MAX, VAL_DST_MIN, VAL_DST_MAX) for val_str in data.split(",")[0:2]]
        # print(val_list)

        mouse_mov_x = 0
        if abs(val_list[1]) > VAL_DST_DEADZONE:
            mov = val_list[1]
            mov_abs = abs(mov)
            sign = mov / mov_abs
            mouse_mov_x = sign * apply_formula(mov_abs, MOV_FORMULA) * MOV_SPEED_MAX / VAL_TX_DELAY_S

        mouse_mov_y = 0
        if abs(val_list[0]) > VAL_DST_DEADZONE:
            mov = val_list[0]
            mov_abs = abs(mov)
            sign = mov / mov_abs
            mouse_mov_y = sign * apply_formula(mov_abs, MOV_FORMULA) * MOV_SPEED_MAX / VAL_TX_DELAY_S

        if mouse_mov_x:
            if abs(mouse_mov_x + mouse_mov_x_buf) < 1:
                mouse_mov_x_buf += mouse_mov_x
            else:
                mouse.move(mouse_mov_x + mouse_mov_x_buf, 0, absolute=False)
                mouse_mov_x_buf = 0
        if mouse_mov_y:
            if abs(mouse_mov_y + mouse_mov_y_buf) < 1:
                mouse_mov_y_buf += mouse_mov_y
            else:
                mouse.move(0, mouse_mov_y + mouse_mov_y_buf, absolute=False)
                mouse_mov_y_buf = 0

if __name__ ==  "__main__":
    main()
