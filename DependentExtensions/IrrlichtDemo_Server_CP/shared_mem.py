__author__ = "Tony Do"
__copyright__ = "vanhuong.do@asicland.com"
__date__ = "2023.05.12"

import sysv_ipc, time

NULL_CHAR = '\0'  

class CShmReader : 
    def __init__( self, key, size ) : 
        self.key = key
        self.size = size
        self.pre_s = ""
        try:
            self.memory = sysv_ipc.SharedMemory(self.key, sysv_ipc.IPC_CREX, size = self.size, mode=0o666)
        except sysv_ipc.ExistentialError:
            print(f"exist shareMemory")
            self.memory = sysv_ipc.SharedMemory(self.key, size = self.size, mode=0o666)
 
    def doReadShm(self) : 
        s = self.memory.read()
        s = s.decode()
        s = s.strip()
        i = s.find(NULL_CHAR)
        if i != -1:
            s = s[:i]
        if not s :
            return "empty", True
        elif s == self.pre_s:
            return "same", True
        self.pre_s = s
        return s, False

    def doWriteShm(self,text):
        text += NULL_CHAR
        text = text.encode()
        self.memory.write(text)

    def clear(self):
        try:
            self.memory.remove()
        except sysv_ipc.InternalError:
            pass
       

class CSemaphore : 
    def __init__( self, key ) : 
        self.key = key
        try:
            self.semaphore = sysv_ipc.Semaphore(self.key, sysv_ipc.IPC_CREX, mode=0o666)
        except sysv_ipc.ExistentialError:
            self.semaphore = sysv_ipc.Semaphore(self.key, mode=0o666)
            print(f"exist semaphore")
 
    def wait(self):
        try:
            self.semaphore.acquire(timeout=0)
            return True
        except sysv_ipc.BusyError:
            return False

    def release(self):
        self.semaphore.release()
    
    def clear(self):
        try:
            self.semaphore.remove()
        except sysv_ipc.InternalError:
            pass

 
if __name__ == '__main__':
    i = 0
    print("\n")
    while True:
        i = i +1
        key_read = 777
        size_read = 1024
        s = CShmReader()
        s.doReadShm( key_read, size_read )
        time.sleep(1)
        key_write = 1234
        size_write = 1024
        w = CShmReader()
        w.doWriteShm(key_write, size_write)
        time.sleep(4)
