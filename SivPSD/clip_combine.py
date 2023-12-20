import sys
import pyautogui
import pyperclip
import time

application_name = 'CLIP STUDIO PAINT'

sleep_time = 0.05

# 「レイヤー名の変更」のショートカットキー
layer_rename_key = ['f2']

# 下のレイヤーへ移動するショートカットキー
layer_down_key = ['alt', '[']

# 「選択中のレイヤーを結合」のショートカットキー
layer_combine_key = ['shift', 'alt', 'E']

# 結合対象にしたいレイヤーの名前に含まれる文字列
combine_target_literal = '#'

# イラストの最底辺のレイヤー
bottom_layer = '用紙'


def simulate_key(keys):
    if len(keys) == 1:
        pyautogui.press(keys[0])
    elif len(keys) == 2:
        pyautogui.hotkey(keys[0], keys[1])
    elif len(keys) == 3:
        pyautogui.hotkey(keys[0], keys[1], keys[2])
    elif len(keys) == 4:
        pyautogui.hotkey(keys[0], keys[1], keys[2], keys[3])


if __name__ == '__main__':
    if sys.platform == "win32":
        pyautogui.getWindowsWithTitle('CLIP STUDIO PAINT')[0].activate()
    else:
        print("Please focus " + application_name + " in 3 seconds...")
        time.sleep(3)

    while True:
        # 「レイヤー名の変更」ウィンドウを出す
        simulate_key(layer_rename_key)

        time.sleep(sleep_time)

        pyautogui.hotkey('ctrl', 'c')

        time.sleep(sleep_time)

        read_layer_name = pyperclip.paste()
        print('Read layer: ' + read_layer_name)

        # ウィンドウを閉じる
        pyautogui.press('escape')
        time.sleep(sleep_time)

        if combine_target_literal in read_layer_name:
            # レイヤーを結合
            simulate_key(layer_combine_key)
            print('\tCombined!')

        if read_layer_name == bottom_layer:
            break

        # 下のレイヤーへ移動
        simulate_key(layer_down_key)
