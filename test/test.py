import base64
import getpass
import os
import socket
import sys
import traceback
from paramiko.py3compat import input
import paramiko
#try:
#    import interactive
#except ImportError:
#    from . import interactive

def manual_auth(private_rsa_key):
    path = os.path.join(os.environ['HOME'], '.ssh', private_rsa_key)
    return path

username = 'root'
hostname = '192.168.7.2'
password = 'whzT72Hi'
private_rsa_key = 'begalbone-black_rsa'

# now, connect and use paramiko Client to negotiate SSH2 across the connection
try:
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.WarningPolicy())
    print('*** Connecting...')
    client.connect(hostname, username='root', password=password,
            key_filename=manual_auth(private_rsa_key))
    transport = client.get_transport()

#    chan = client.invoke_shell()
#    interactive.interactive_shell(chan)
    stdin, stdout, stderr = client.exec_command('uname -r')

    stdout.read()
#    chan.close()
    client.close()

except Exception as e:
    print('*** Caught exception: %s: %s' % (e.__class__, e))
    traceback.print_exc()
    try:
        client.close()
    except:
        pass
    sys.exit(1)
