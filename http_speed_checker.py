import time
import requests
import sys

if __name__ == '__main__':
    arg = sys.argv
    if len(arg) != 2:
        print('Usage: python http_speed_checker.py [port]')
    port = arg[1]
    rptime = 100

    stime = time.time()
    for i in range(rptime):
        data = requests.get('http://localhost:' + port)
    etime = time.time()

    one_action_time = (etime - stime) / rptime
    print('Elapsed seconds per one request: ' + str(one_action_time))
    