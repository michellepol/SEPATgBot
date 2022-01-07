#!/usr/bin/env python3

import os
import sys

from config import *

import re
import cv2
import time
import json
import base64
import shutil
import datetime
import threading
import numpy as np
from PIL import ImageColor

from ai import *
from tricks import *


sketch_upload_pool = []
painting_pool = []
options_str = '{"alpha":0,"points":[],"lineColor":[0,0,0],"line":false,"hasReference":false}'


def get_request_image(fname):
    img = np.fromfile(fname, dtype=np.uint8)
    img = cv2.imdecode(img, -1)
    return img


def handle_sketch_upload_pool():
    room, sketch, method = sketch_upload_pool[0]
    del sketch_upload_pool[0]
    room_path = 'game/rooms/' + room
    print('processing sketch in ' + room_path)
    improved_sketch = sketch.copy()
    improved_sketch = min_resize(improved_sketch, 512)
    improved_sketch = cv_denoise(improved_sketch)
    improved_sketch = sensitive(improved_sketch, s=5.0)
    improved_sketch = go_tail(improved_sketch)
    cv2.imwrite(room_path + '/sketch.improved.jpg', improved_sketch)
    color_sketch = improved_sketch.copy()
    std = cal_std(color_sketch)
    print('std = ' + str(std))
    print('max value = ' + str(np.max(color_sketch)))
    need_de_painting = (std > 100.0) and method == 'rendering'
    if method=='recolorization' or need_de_painting:
        improved_sketch = go_passline(color_sketch)
        improved_sketch = min_k_down_c(improved_sketch, 2)
        improved_sketch = cv_denoise(improved_sketch)
        improved_sketch = go_tail(improved_sketch)
        improved_sketch = sensitive(improved_sketch, s=5.0)
        cv2.imwrite(room_path + '/sketch.recolorization.jpg', min_black(improved_sketch))
        if need_de_painting:
            cv2.imwrite(room_path + '/sketch.de_painting.jpg', min_black(improved_sketch))
            print('In rendering mode, the user has uploaded a painting, and I have translated it into a sketch.')
        print('sketch lined')
    cv2.imwrite(room_path + '/sketch.colorization.jpg', min_black(color_sketch))
    cv2.imwrite(room_path + '/sketch.rendering.jpg', eye_black(color_sketch))
    print('sketch improved')


def handle_painting_pool():
    room, ID, sketch, alpha, reference, points, method, lineColor, line = painting_pool[0]
    del painting_pool[0]
    room_path = 'game/rooms/' + room
    print('processing painting in ' + room_path)
    sketch_1024 = k_resize(sketch, 64)
    if os.path.exists(room_path + '/sketch.de_painting.jpg') and method == 'rendering':
        vice_sketch_1024 = k_resize(cv2.imread(room_path + '/sketch.de_painting.jpg', cv2.IMREAD_GRAYSCALE), 64)
        sketch_256 = mini_norm(k_resize(min_k_down(vice_sketch_1024, 2), 16))
        sketch_128 = hard_norm(sk_resize(min_k_down(vice_sketch_1024, 4), 32))
    else:
        sketch_256 = mini_norm(k_resize(min_k_down(sketch_1024, 2), 16))
        sketch_128 = hard_norm(sk_resize(min_k_down(sketch_1024, 4), 32))
    print('sketch prepared')
    if debugging:
        cv2.imwrite(room_path + '/sketch.128.jpg', sketch_128)
        cv2.imwrite(room_path + '/sketch.256.jpg', sketch_256)
    baby = go_baby(sketch_128, opreate_normal_hint(ini_hint(sketch_128), points, type=0, length=1))
    baby = de_line(baby, sketch_128)
    for _ in range(16):
        baby = blur_line(baby, sketch_128)
    baby = go_tail(baby)
    baby = clip_15(baby)
    if debugging:
        cv2.imwrite(room_path + '/baby.' + ID + '.jpg', baby)
    print('baby born')
    composition = go_gird(sketch=sketch_256, latent=d_resize(baby, sketch_256.shape), hint=ini_hint(sketch_256))
    if line:
        composition = emph_line(composition, d_resize(min_k_down(sketch_1024, 2), composition.shape), lineColor)
    composition = go_tail(composition)
    cv2.imwrite(room_path + '/composition.' + ID + '.jpg', composition)
    print('composition saved')
    painting_function = go_head
    if method == 'rendering':
        painting_function = go_neck
    print('method: ' + method)
    result = painting_function(
        sketch=sketch_1024,
        global_hint=k_resize(composition, 14),
        local_hint=opreate_normal_hint(ini_hint(sketch_1024), points, type=2, length=2),
        global_hint_x=k_resize(reference, 14) if reference is not None else k_resize(composition, 14),
        alpha=(1 - alpha) if reference is not None else 1
    )
    result = go_tail(result)
    cv2.imwrite(room_path + '/result.' + ID + '.jpg', result)
    if debugging:
        cv2.imwrite(room_path + '/icon.' + ID + '.jpg', max_resize(result, 128))
    return room_path + '/result.' + ID + '.jpg'


def upload_sketch(inputfilename, method):
    ID = datetime.datetime.now().strftime('H%HM%MS%S')
    room = datetime.datetime.now().strftime('%b%dH%HM%MS%S') + 'R' + str(np.random.randint(100, 999))
    room_path = 'game/rooms/' + room
    os.makedirs(room_path, exist_ok=True)
    sketch = from_png_to_jpg(get_request_image(inputfilename))
    cv2.imwrite(room_path + '/sketch.original.jpg', sketch)
    print('original_sketch saved')
    print('sketch upload pool get request: ' + method)
    sketch_upload_pool.append((room, sketch, method))
    return room


def request_result(room, method, points):
    ID = datetime.datetime.now().strftime('H%HM%MS%S')
    room_path = 'game/rooms/' + room
    if debugging:
        with open(room_path + '/options.' + ID + '.json', 'w') as f:
            f.write(options_str)
    options = json.loads(options_str)
    sketch = cv2.imread(room_path + '/sketch.' + method + '.jpg', cv2.IMREAD_GRAYSCALE)
    alpha = float(options["alpha"])
    reference = None
    print('request result room = ' + str(room) + ', ID = ' + str(ID))
    lineColor = np.array(options["lineColor"])
    line = options["line"]
    painting_pool.append([room, ID, sketch, alpha, reference, points, method, lineColor, line])


os.makedirs('game/rooms', exist_ok=True)

method = 'colorization'

def predict(mylist = []):
    points = []
    inverted_points = []
    for item in mylist:
        print('recieved item' , item)
        x = item[0]
        y = item[1]
        rgb = ImageColor.getcolor(item[2], "RGB")
        r = rgb[0]
        g = rgb[1]
        b = rgb[2]
        inv_r = 255 - r
        inv_g = 255 - g
        inv_b = 255 - b
        inverted_points.append([x, y, inv_r, inv_g, inv_b, 0])
        points.append([x, y, r, g, b, 0])
    room = upload_sketch('../in.jpg', method)
    handle_sketch_upload_pool()
    request_result(room, method, points)
    result = handle_painting_pool()
    request_result(room, method, inverted_points)
    result2 = handle_painting_pool()
    shutil.copyfile(result, '../out.jpg')
    shutil.copyfile(result2, '../inverted_out.jpg')






