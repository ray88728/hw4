import serial

import time

import locale

import matplotlib.pyplot as plt

import numpy as np

# XBee setting

serdev = '/dev/ttyUSB2'

s = serial.Serial(serdev, 9600)


s.write("+++".encode())

char = s.read(2)

print("Enter AT mode.")

print(char.decode())


s.write("ATMY 0x00\r\n".encode())

char = s.read(3)

print("Set MY 0x00.")

print(char.decode())


s.write("ATDL 0xFFFF\r\n".encode())

char = s.read(3)

print("Set DL 0xFFFF.")

print(char.decode())


s.write("ATID 0x4041\r\n".encode())

char = s.read(3)

print("Set PAN ID 0x4041.")

print(char.decode())


s.write("ATWR\r\n".encode())

char = s.read(3)

print("Write config.")

print(char.decode())

s.write('ATWR\r\n'.encode())

char = s.read(3)

print('Write config.')

print(char.decode())


s.write('ATCN\r\n'.encode())

char = s.read(3)

print('Exit AT mode.')

print(char.decode())

x=[]
print("start sending RPC")

time_stamp=np.array([0,1,2,3,4,5,6,7,8,9,10.11,12,13,14,15,16,17,18,19,20])

query_time =0
y=''
while query_time <= 21:
    send="/getAcc/run\r"
    s.write(send.encode())
    while(1):

        print("1"+"\n")
        line = s.read(1).decode()
        #print(line+"\n")
        if (line=='\n')or(line=="\r"):
            break
        else:
            y=y+line
            print(y)

    print(y)

    x.append(y)

    print('')

    print(query_time)

    y=''

    query_time = query_time + 1
    time.sleep(1)

fig, ax = plt.subplots(1, 1)

l1,=ax.plot(time_stamp,x)


ax.set_xlabel('timestamp')

ax.set_ylabel('acc value')

ax.legend((l1),('X'))


plt.show()


s.close()