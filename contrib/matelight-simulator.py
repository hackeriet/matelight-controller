#!/usr/bin/env python3

import argparse
import os
import socket
import queue
import threading
import time
import datetime
import struct
import tkinter
import tkinter.ttk

DEFAULT_WIDTH = 10
DEFAULT_HEIGHT = 20
UDP_PORT = 21324
JS_EVENT_BUTTON = 0x01
JS_EVENT_AXIS = 0x02
JS_EVENT_INIT = 0x80
CIRCLE_RADIUS = 18


# https://kno.wled.ge/interfaces/udp-realtime/
class WLED:
    def __init__(self, sock, width=DEFAULT_WIDTH, height=DEFAULT_HEIGHT):
        self.sock = sock
        self.width = width
        self.height = height
        self.pixels = [(0, 0, 0) for _ in range(self.width * self.height)]
        self.pixel_data = False
        self.pixels_active = False
        self.pixels_updated = True
        self.queue = queue.Queue()
        self.sender = None
        self.protocol = None
        self.timeout = None
        self.last_update = None
        self.update_interval = None
        self.pixels_expire = None

    def udp_receiver(self):
        while True:
            (data, address) = self.sock.recvfrom(65536)
            self.queue.put((data, address))

    def udp_process(self):
        while not self.queue.empty():
            (data, address) = self.queue.get()
            if address:
                self.sender = address[0]
            else:
                self.sender = None
            now = time.time()
            if len(data) < 2:
                continue
            if self.last_update:
                self.update_interval = now - self.last_update
            else:
                self.update_interval = None
            self.last_update = now
            protocol_num = data[0]
            self.timeout = data[1]
            offset = 2
            if self.timeout == 255:
                self.timeout = None
                self.pixels_expire = None
            else:
                self.pixels_expire = now + self.timeout
            if protocol_num == 1:
                self.protocol = 'WARLS'
                while (offset + 4) <= len(data):
                    idx = data[offset + 0]
                    r, g, b = tuple(data[offset + 1:offset + 4])
                    if idx < len(self.pixels):
                        self.pixels[idx] = (r, g, b)
                        self.pixel_data = True
                        self.pixels_updated = True
                    offset += 4
            elif protocol_num == 2:
                self.protocol = 'DRGB'
                idx = 0
                while (offset + 3) <= len(data):
                    r, g, b = tuple(data[offset + 0:offset + 3])
                    if idx < len(self.pixels):
                        self.pixels[idx] = (r, g, b)
                        self.pixel_data = True
                        self.pixels_updated = True
                    offset += 3
                    idx += 1
            elif protocol_num == 3:
                self.protocol = 'DRGBW'
                idx = 0
                while (offset + 4) <= len(data):
                    r, g, b, w = tuple(data[offset + 0:offset + 4])
                    if idx < len(self.pixels):
                        self.pixels[idx] = (r, g, b)
                        self.pixel_data = True
                        self.pixels_updated = True
                    offset += 4
                    idx += 1
            elif protocol_num == 4:
                if len(data) >= 4:
                    self.protocol = 'DNRGB'
                    idx_high, idx_low = tuple(data[offset + 0:offset + 2])
                    idx = (idx_high * 256) + idx_low
                    offset += 2
                    while (offset + 3) <= len(data):
                        r, g, b = tuple(data[offset + 0:offset + 3])
                        if idx < len(self.pixels):
                            self.pixels[idx] = (r, g, b)
                            self.pixel_data = True
                            self.pixels_updated = True
                        offset += 3
                        idx += 1
            else:
                self.protocol = f'Unknown ({protocol_num})'

    def update_state(self):
        now = time.time()
        active = self.pixel_data and (not self.pixels_expire or now < self.pixels_expire)
        if self.pixels_active != active:
            self.pixels_active = active
            self.pixels_updated = True

    def reset_pixels_updated(self):
        self.pixels_updated = False


# https://www.kernel.org/doc/Documentation/input/joystick-api.txt
class FifoJoypad:
    def __init__(self, path=None):
        self.path = path
        self.f = None
        self.queue = queue.Queue()

    def fifo_writer(self):
        if not self.f and self.path:
            self.f = open(self.path, mode='wb', buffering=0)
        while True:
            event = self.queue.get()
            if not self.f and self.path:
                self.f = open(self.path, mode='wb', buffering=0)
            if self.f:
                self.f.write(event)
                self.f.flush()

    def axis(self, number, value):
        self.queue.put(struct.pack('IhBB', int(time.time()), value, JS_EVENT_AXIS, number))

    def button(self, number, value):
        self.queue.put(struct.pack('IhBB', int(time.time()), 1 if value else 0, JS_EVENT_BUTTON, number))


class Window:
    def __init__(self, wled, joypad):
        self.wled = wled
        self.joypad = joypad

        self.root = tkinter.Tk()
        self.root.title('Matelight Simulator')

        self.frame = tkinter.ttk.Frame(self.root, padding=10)
        self.frame.grid()

        self.canvas = tkinter.Canvas(self.frame, bg='#ffffff', height=wled.height*((CIRCLE_RADIUS * 2) + 1), width=wled.width*((CIRCLE_RADIUS * 2) + 1))
        self.canvas.grid(row=0, column=0, columnspan=9, sticky='nw')

        buttons = [
            ('left',    'Left',     2, 0),
            ('right',   'Right',    2, 2),
            ('up',      'Up',       1, 1),
            ('down',    'Down',     3, 1),
            ('select',  'Select',   2, 4),
            ('start',   'Start',    2, 5),
            ('b',       'B',        2, 7),
            ('a',       'A',        2, 8),
        ]

        for name, text, row, column in buttons:
            button = tkinter.ttk.Button(self.frame, text=text)
            button.grid(row=row, column=column, sticky='nw')
            button.bind('<Button>', lambda event, name=name: self.button_down(event, name))
            button.bind('<ButtonRelease>', lambda event, name=name: self.button_up(event, name))

        self.label = tkinter.ttk.Label(self.frame, text='')
        self.label.grid(row=4, column=0, columnspan=8, sticky='sw')
        tkinter.ttk.Button(self.frame, text='Quit', command=self.quit).grid(row=4, column=8, sticky='se')

        self.root.bind('<Key>', self.key_down)
        self.root.bind('<KeyRelease>', self.key_up)

    def update_canvas(self):
        if not self.wled.pixels_updated:
            return
        self.canvas.delete('all')
        for idx in range(0, self.wled.width * self.wled.height):
            y = idx // self.wled.width
            x = idx % self.wled.width
            canvas_y = y * ((CIRCLE_RADIUS * 2) + 1)
            canvas_x = x * ((CIRCLE_RADIUS * 2) + 1)
            if self.wled.pixels_active:
                r, g, b = self.wled.pixels[idx]
            else:
                if (((y % 2) + x) % 2) == 0:
                    r = g = b = 0x00
                else:
                    r = g = b = 0xff
            x0 = CIRCLE_RADIUS + canvas_x - CIRCLE_RADIUS
            y0 = CIRCLE_RADIUS + canvas_y - CIRCLE_RADIUS
            x1 = CIRCLE_RADIUS + canvas_x + CIRCLE_RADIUS
            y1 = CIRCLE_RADIUS + canvas_y + CIRCLE_RADIUS
            self.canvas.create_oval(x0, y0, x1, y1, fill=f'#{r:02x}{g:02x}{b:02x}', outline='#000000', width=1)
        self.canvas.update()
        self.wled.reset_pixels_updated()

    def update_label(self):
        now = time.time()
        label_text = ''
        if self.wled.pixels_active:
            label_text = f'Active. Source: {self.wled.sender}. {self.wled.protocol}.'
            if self.wled.timeout:
                label_text += f' Timeout: {self.wled.timeout}.'
            else:
                label_text += ' No timeout.'
            if self.wled.update_interval:
                updates_per_sec = 1.0 / self.wled.update_interval
                label_text += f' Updates/sec: {updates_per_sec:.2f}'
        else:
            label_text = 'No source.'
            if self.wled.sender:
                label_text += f' Last source: {self.wled.sender}.'
            if self.wled.last_update:
                if (now - self.wled.last_update) < 60:
                    interval = int(now - self.wled.last_update)
                    if interval == 1:
                        label_text += f' Last Update: {interval} sec ago.'
                    else:
                        label_text += f' Last Update: {interval} secs ago.'
                else:
                    last_pixels_fmt = datetime.datetime.fromtimestamp(self.wled.last_update).strftime('%Y-%m-%d %H:%M:%S')
                    label_text += f' Last Update: {last_pixels_fmt}.'
        self.label.config(text=label_text)
        self.label.update()

    def periodic(self):
        try:
            self.wled.udp_process()
            self.wled.update_state()
            self.update_canvas()
            self.update_label()
        finally:
            self.root.after(int(1000/60), self.periodic)

    def button(self, name, value):
        if name == 'left':
            self.joypad.axis(0, -32768 if value else 0)
        elif name == 'right':
            self.joypad.axis(0, 32767 if value else 0)
        elif name == 'up':
            self.joypad.axis(1, -32768 if value else 0)
        elif name == 'down':
            self.joypad.axis(1, 32767 if value else 0)
        elif name == 'select':
            self.joypad.button(8, value)
        elif name == 'start':
            self.joypad.button(9, value)
        elif name == 'b':
            self.joypad.button(0, value)
        elif name == 'a':
            self.joypad.button(1, value)

    def button_down(self, event, name):
        self.button(name, True)

    def button_up(self, event, name):
        self.button(name, False)

    def key(self, event, value):
        if event.keysym in ['a', 'A', 'Left', 'KP_Left']:
            self.button('left', value)
        elif event.keysym in ['d', 'D', 'Right', 'KP_Right']:
            self.button('right', value)
        elif event.keysym in ['w', 'W', 'Up', 'KP_Up']:
            self.button('up', value)
        elif event.keysym in ['s', 'S', 'Down', 'KP_Down']:
            self.button('down', value)
        elif event.keysym in ['Shift_L', 'Shift_R']:
            self.button('select', value)
        elif event.keysym in ['Return', 'KP_Enter']:
            self.button('start', value)
        elif event.keysym in ['z', 'Z']:
            self.button('b', value)
        elif event.keysym in ['x', 'X']:
            self.button('a', value)

    def key_down(self, event):
        self.key(event, True)

    def key_up(self, event):
        self.key(event, False)

    def quit(self):
        self.root.destroy()


parser = argparse.ArgumentParser()
parser.add_argument('--width', help='Grid width', type=int, default=DEFAULT_WIDTH)
parser.add_argument('--height', help='Grid height', type=int, default=DEFAULT_HEIGHT)
parser.add_argument('--address', help='Listen address', default='127.0.0.1')
parser.add_argument('--port', help='Listen port', type=int, default=UDP_PORT)
parser.add_argument('--fifo', help='Joypad FIFO')
args = parser.parse_args()

sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
sock.bind((args.address, args.port))

if args.fifo and not os.path.exists(args.fifo):
    os.mkfifo(args.fifo)

if args.fifo and os.path.exists(args.fifo):
    joypad = FifoJoypad(args.fifo)
else:
    joypad = FifoJoypad()

wled = WLED(sock, width=args.width, height=args.height)

udp_thread = threading.Thread(target=wled.udp_receiver, name='udp-receiver', daemon=True)
udp_thread.start()

fifo_thread = threading.Thread(target=joypad.fifo_writer, name='fifo-writer', daemon=True)
fifo_thread.start()

window = Window(wled, joypad)
window.periodic()
window.root.mainloop()
