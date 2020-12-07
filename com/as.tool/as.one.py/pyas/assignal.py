__lic__ = '''
/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2015  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
 '''

import os,sys,time
import threading
import multiprocessing
from bitarray import bitarray
from pyas.can import *

__all__ = ['Network', 'QView']



# big endian bits map
_bebm = []

#cstr = 'COM big endian bits map:\n'
for i in range(64):
#    cstr += '\n\tB%2d '%(i)
    for j in range(8):
        _bebm.append(i*8 + 7-j)
#        cstr += '%3d '%(i*8 + 7-j)
#print(cstr)

class Sdu():
    def __init__(self, length):
        self.data = []
        for i in range(0,length):
            self.data.append(0x5A)

    def __iter__(self):
        for v in self.data:
            yield v

    def __len__(self):
        return len(self.data)

    def beset(self, start, size, value):
        rBit = size-1
        nBit = _bebm.index(start)
        wByte = 0
        wBit = 0
        for i in range(size):
            wBit = _bebm[nBit]
            wByte = int(wBit/8)
            wBit  = wBit%8
            if(value&(1<<rBit) != 0):
                self.data[wByte] |= 1<<wBit
            else:
                self.data[wByte] &= ~(1<<wBit)
            nBit += 1
            rBit -= 1

    def leset(self, start, size, value):
        rBit = size-1
        nBit = start+size-1
        wByte = 0
        wBit = 0
        for i in range(size):
            wBit = nBit
            wByte = int(wBit/8)
            wBit  = wBit%8
            if(value&(1<<rBit) != 0):
                self.data[wByte] |= 1<<wBit
            else:
                self.data[wByte] &= ~(1<<wBit)
            nBit -= 1
            rBit -= 1

    def set(self, sig, value):
        start = sig['start']
        size = sig['size']
        endian = sig['endian'] # 1:Little 0: BIG
        if(endian == 0):
            self.beset(start, size, value)
        else:
            self.leset(start, size, value)

    def beget(self, start, size):
        nBit = _bebm.index(start)
        value = 0
        for i in range(size):
            rBit = _bebm[nBit]
            rByte = int(rBit/8)
            if(self.data[rByte]&(1<<(rBit%8)) != 0):
                value = (value<<1)+1
            else:
                value = (value<<1)+0
            nBit += 1
        return value

    def leget(self, start, size):
        nBit = start+size-1
        value = 0
        for i in range(size):
            rBit = nBit
            rByte = int(rBit/8)
            if(self.data[rByte]&(1<<(rBit%8)) != 0):
                value = (value<<1)+1
            else:
                value = (value<<1)+0
            nBit -= 1
        return value

    def get(self, sig):
        # for big endian only
        start = sig['start']
        size = sig['size']
        endian = sig['endian'] # 1:Little 0: BIG
        if(endian == 0):
            return self.beget(start, size)
        else:
            return self.leget(start, size)

    def __str__(self):
        cstr = ''
        for b in self.data:
            cstr += '%02X'%(b)
        return cstr

class Signal():
    def __init__(self, sg):
        self.sg = sg
        self.mask = (1<<sg['size'])-1
        self.notifyMQ = []
        self.set_value(0)

    def register_mq(self, mq):
        self.notifyMQ.append(mq)

    def unregister_mq(self, mq):
        self.notifyMQ.remove(mq)

    def get_max(self):
        return 0xFFFFFFFF&self.mask

    def get_min(self):
        return 0

    def set_value(self, v):
        self.value = v&self.mask
        for mq in self.notifyMQ:
            mq.put(self.value)

    def get_value(self):
        return self.value&self.mask

    def __str__(self):
        return str(self.sg)

    def __getitem__(self, key):
        return  self.sg[key]

class Message():
    def __init__(self, msg, busid):
        self.msg = msg
        self.busid = busid
        self.sgs = {}
        self.sdu = Sdu(msg['length'])
        if('period' in msg):
            self.period = msg['period']
        else:
            self.period = 1000
        self.timer = time.time()
        for sg in msg['sgList']:
            sg = sg['sg']
            self.sgs[sg['name']] = Signal(sg)

    def attrib(self, key):
        return self.msg[key]

    def set_period(self, period):
        self.period = period

    def get_period(self):
        return self.period

    def transmit(self):
        for sig in self:
            self.sdu.set(sig, sig.value)
        ercd = can_write(self.busid, self.msg['id'], self.sdu)
        if(ercd == False):
            print('cansend can%s %03X#%s failed'%(self.busid, self.msg['id'], self.sdu))

    def ProcessTX(self):
        if(self.period <= 0): return
        elapsed = time.time() - self.timer
        if(self.period <= elapsed*1000):
            self.timer = time.time()
            self.transmit()

    def ProcessRX(self):
        result,canid,data = can_read(self.busid, self.msg['id'])
        if(result):
            self.sdu.data = [d for d in data]
            for sig in self:
                sig.value = self.sdu.get(sig)

    def IsTransmit(self):
        if(self.msg['node'] != 'AS'):
            return True
        return False

    def Process(self):
        if(self.msg['node'] != 'AS'):
            self.ProcessTX()
        self.ProcessRX()

    def __str__(self):
        return str(self.msg)

    def __iter__(self):
        for key,sig in self.sgs.items():
            yield sig

    def __getitem__(self, key):
        self.sgs[key].value = self.sdu.get(self.sgs[key])
        return  self.sgs[key].value

    def __setitem__(self, key, value):
        self.sgs[key].set_value(value)

    def __getattr__(self, key):
        return  self.__getitem__(key)

    def __setattr__(self, key, value):
        if(key in ['msg','busid','sgs','sdu','period','timer']):
            self.__dict__[key] = value
        else:
            self.__setitem__(key, value)

class Network(threading.Thread):
    def __init__(self, dbcf, busid=DFTBUS):
        threading.Thread.__init__(self)
        dbc = self.parseCANDBC(dbcf)
        self.msgs = {}
        for msg in dbc['boList']:
            msg = msg['bo']
            self.msgs[msg['name']] = Message(msg, busid)
        self.start()

    def lookup(self, name):
        for msg in self:
            for sig in msg:
                if(sig['name'] == name):
                    return sig
        return None

    def stop(self):
        self.is_running = False

    def run(self):
        self.is_running = True
        while(self.is_running):
            for msg in self:
                msg.Process()
            time.sleep(0.001)

    def __iter__(self):
        for key,msg in self.msgs.items():
            yield msg

    def __getitem__(self, key):
        return self.msgs[key]

    def __getattr__(self, key):
        return self.__getitem__(key)

    def parseCANDBC(self, dbc):
        pydbc = os.path.abspath('../py.can.database.access/ascc')
        assert(os.path.exists(pydbc))
        sys.path.append(pydbc)
        import cc.ascp as ascp
        return ascp.parse(dbc)

class View(object):
    def __init__(self, sig, ax, scale=1, offset=0):
        self.sig = sig
        self.ax = ax
        self.scale = scale
        self.offset = offset
        self.ymax = 1
        self.tmax = 10
        self.st = time.time()
        self.tdata = []
        self.ydata = []
        self.line, = self.ax.plot([], [], lw=2)
        self.ax.grid()
        self.ax.set_ylim(self.sig.get_min(), self.sig.get_max())
        self.ax.set_xlim(0, self.tmax)

    def emitter(self):
        while True:
            yield self.sig.get_value()*self.scale+self.offset

    def update(self, y):
        t = time.time() - self.st
        self.tdata.append(t)
        self.ydata.append(y)

        bDraw = False
        if(t > self.tmax):
            self.tmax += 10
            self.ax.set_xlim(0, self.tmax)
            bDraw = True

        if(y > self.ymax):
            self.ymax = y
            self.ax.set_ylim(0, 1.1*self.ymax)
            bDraw = True

        if(bDraw):
            self.ax.figure.canvas.draw()

        self.line.set_data(self.tdata, self.ydata)
        return self.line,

class QView(multiprocessing.Process):
    def __init__(self, sig, scale=1, offset=0):
        self.mq = multiprocessing.Queue()
        multiprocessing.Process.__init__(self)
        self.sig = sig
        self.scale = scale
        self.offset = offset
        self.lastValue = 0
        self.sig.register_mq(self.mq)
        self.start()

    def get_value(self):
        try:
            v = self.mq.get_nowait()
            self.lastValue = v
        except:
            v = self.lastValue
        return v*self.scale+self.offset

    def get_max(self):
        if(self.scale > 0):
            return self.sig.get_max()*self.scale+self.offset
        else:
            return self.sig.get_min()*self.scale+self.offset

    def get_min(self):
        if(self.scale < 0):
            return self.sig.get_max()*self.scale+self.offset
        else:
            return self.sig.get_min()*self.scale+self.offset

    def run(self):
        print('subplot for %s'%(self.sig['name']))
        import matplotlib.pyplot as plt
        import matplotlib.animation as animation
        fig, ax = plt.subplots()
        fig.suptitle(self.sig['name'])
        self.view = View(self, ax, self.scale, self.offset)
        self.ani = animation.FuncAnimation(fig, self.view.update, self.view.emitter, interval=10, blit=False, repeat=False)
        plt.show()
        self.sig.unregister_mq(self.mq)

if(__name__ == '__main__'):
    import argparse
    import atexit

    import matplotlib.pyplot as plt
    import matplotlib.animation as animation

    parser = argparse.ArgumentParser(description='view signal value by matplotlib scope')
    parser.add_argument('-b', '--busid', help='can bus id', type=int, default=0, required=False)
    parser.add_argument('-p', '--port', help='can bus port', type=int, default=0, required=False)
    parser.add_argument('-t', '--type', help='can bus type', type=str, default='socket', required=False)
    parser.add_argument('--baudrate', help='can bus baudrate', type=int, default=1000000, required=False)
    parser.add_argument('-n', '--network', help='can network(*.dbc)', type=str, required=True)
    parser.add_argument('-v', '--view', help='list of signals to be viewed', type=str, nargs='+', required=True)
    args = parser.parse_args()

    can_open(args.busid, args.type, args.port, args.baudrate)
    nt = Network(args.network)

    @atexit.register
    def goodbye():
        nt.stop()

    aniList = []
    for v in args.view:
        sig = nt.lookup(v)
        if(sig != None):
            print('view signal %s'%(v))
            fig, ax = plt.subplots()
            fig.suptitle(v)
            vv = View(sig, ax)
            ani = animation.FuncAnimation(fig, vv.update, vv.emitter, interval=10, blit=False, repeat=False)
            aniList.append(ani)
        else:
            print('could find signal %s'%(v))

    if(len(aniList) > 0):
        plt.show()

    nt.stop()

