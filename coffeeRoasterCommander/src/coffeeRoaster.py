import requests
import signal
import sys

##
class Config():
    def __init__(self,inputJson):
        self.P = inputJson['P']
        self.I = inputJson['I']
        self.D = inputJson['D']
        self.setpoint = inputJson['setpoint']
        self.fan = inputJson['fan']
        self.on = inputJson['on']
        self.json = inputJson
    def print(self):
        print(self.json)
    def update(self):
        self.json['P'] = self.P
        self.json['I'] = self.I
        self.json['D'] = self.D
        self.json['setpoint'] = self.setpoint
        self.json['fan'] = self.fan
        self.json['on'] = self.on

URL = 'http://192.168.2.190/'
r = requests.get(URL)
print( r.content )

r = requests.get(URL + 'config')
print( r.content )
config = Config(r.json())

config.print()
def sig_handler(sig,frame):
    print("exiting")
    sys.exit(0)


signal.signal(signal.SIGINT,sig_handler)

def processInput(myInput):
    if myInput == 'exit':
        sys.exit(0)
    elif myInput.startswith('get'):
        getValue(myInput[4:])
    elif myInput.startswith('set'):
        setValue(myInput[4:])
    else:
        print("invalid command: ",myInput)

def getValue(myInput):
    r = requests.get(URL + 'c')
    global config
    config = Config(r.json())
    if myInput.startswith('p'):
        print("Proportional Term: ", config.P)
    elif myInput.startswith('i'):
        print("Integral Term: ", config.I)
    elif myInput.startswith('d'):
        print("Derivative Term: ", config.D)
    elif myInput.startswith('s'):
        print("setpoint: ", config.setpoint)
    elif myInput.startswith('f'):
        print("fan speed: ", config.fan*100, "%")
    elif myInput.startswith('c'):
        config.print()
    else:
        print("invalid get request! ", myInput)

def setValue(myInput):
    global config
    arr = myInput.split()
    if myInput.startswith('p'):
        config.P = float(arr[1])
    elif myInput.startswith('i'):
        config.I = float(arr[1])
    elif myInput.startswith('d'):
        config.D = float(arr[1])
    elif myInput.startswith('s'):
        config.setpoint = float(arr[1])
    elif myInput.startswith('f'):
        config.fan = float(arr[1])
    elif myInput.startswith('on'):
        config.on = True
    elif myInput.startswith('off'):
        config.on = False
    else:
        print("invalid set request! ", myInput)
    config.update()
    print("new config: ")
    config.print()
    msg = config.json
    msg['command'] = 'SET CONFIG'
    requests.post(URL + "set",json=msg)

if __name__ == '__main__':
    while True:
        myInput = input()
        print("read: ", myInput)
        processInput(myInput)
