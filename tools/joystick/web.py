#!/usr/bin/env python3
import time
import threading
from flask import Flask

import cereal.messaging as messaging

app = Flask(__name__)
pm = messaging.PubMaster(['testJoystick'])

index = """
<html>
<head>
<script src="https://github.com/bobboteck/JoyStick/releases/download/v1.1.6/joy.min.js"></script>
</head>
<body>
<div id="joyDiv" style="width:100%;height:100%"></div>
<script type="text/javascript">
// Create JoyStick object into the DIV 'joyDiv'
var joy = new JoyStick('joyDiv');
function sendMoveCommand(x,y) {
  let xhr = new XMLHttpRequest();
  xhr.open("GET", "/control/"+x+"/"+y);
  xhr.send();
}
function joyFunction(){
  var x = -joy.GetX()/100;
  var y = joy.GetY()/100;
  sendMoveCommand(x,y);
}
var joyInterval = null;
function startJoy() {
    joyInterval = setInterval(joyFunction, 50);
}
function stopJoy() {
    if (joyInterval) {
        clearInterval(joyInterval);
        joyInterval = null;
    }
}
let lastKey = null;
let lastValue = null;
const accelerator = (k)=>{
    if (lastKey != k) {
        lastValue = 0.1;
        lastKey = k;
    }
    return ()=>{
        let a = lastValue*1.027;
        let value = a < 0.89 ? a : lastValue;
        lastValue = value;
        return value;
    }
}
function forward(a) {
sendMoveCommand(0, a())
}
function left(a) {
sendMoveCommand(a(), 0)
}
function right(a) {
sendMoveCommand(-a(), 0)
}
function backward(a) {
sendMoveCommand(0, -a())
}
function keyDownHandler() {
    stopJoy();
    if (!event) return;
    let accel = accelerator(event.key);
    if (event.key === 'w' || event.key === 'ArrowUp')
        forward(accel);
    else if (event.key === 's' || event.key === 'ArrowDown')
        backward(accel);
    else if (event.key === 'a' || event.key === 'ArrowLeft')
        left(accel);
    else if (event.key === 'd' || event.key === 'ArrowRight')
        right(accel);
    else
        console.log('unhandled key:', event.key);
    startJoy();
}
document.addEventListener('keydown', keyDownHandler, true);
startJoy();
</script>
"""

@app.route("/")
def hello_world():
  return index

last_send_time = time.monotonic()
@app.route("/control/<x>/<y>")
def control(x, y):
  global last_send_time
  x,y = float(x), float(y)
  x = max(-1, min(1, x))
  y = max(-1, min(1, y))
  dat = messaging.new_message('testJoystick')
  dat.testJoystick.axes = [y,x]
  dat.testJoystick.buttons = [False]
  pm.send('testJoystick', dat)
  last_send_time = time.monotonic()
  return ""

def handle_timeout():
  while 1:
    this_time = time.monotonic()
    if (last_send_time+0.5) < this_time:
      #print("timeout, no web in %.2f s" % (this_time-last_send_time))
      dat = messaging.new_message('testJoystick')
      dat.testJoystick.axes = [0,0]
      dat.testJoystick.buttons = [False]
      pm.send('testJoystick', dat)
    time.sleep(0.1)

def main():
  threading.Thread(target=handle_timeout, daemon=True).start()
  app.run(host="0.0.0.0")

if __name__ == '__main__':
  main()
