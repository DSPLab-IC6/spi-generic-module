import base64
import getpass
import os
import socket
import sys
import traceback
import re
from paramiko.py3compat import input
import paramiko
#try:
#    import interactive
#except ImportError:
#    from . import interactive

username = 'root'
hostname = '192.168.7.2'
password = 'whzT72Hi'
private_rsa_key = 'begalbone-black_rsa'

class ShellHandler:

    def __init__(self, ssh_client, finish_str='exit status'):
        self.finish_str = finish_str
        self.finish_cmd = 'echo {} $?'.format(self.finish_str)
        self.ssh_client = ssh_client
        self.channel = self.ssh_client.invoke_shell()

        self.connect()

    def __del__(self):
        self.disconnect()

    def connect(self):
        try:
            self.stdin.__iter__()
        except (AttributeError, ValueError) as e:
            self.stdin = self.channel.makefile('wb')
            self.stdout = self.channel.makefile('r')
            self.execute('hostname')

    def disconnect(self):
        self.stdout.close()
        self.stdin.close()

    def execute(self, cmd):
        """
        :param cmd: the command to be executed on the remote computer
        :examples:  execute('ls')
                    execute('finger')
                    execute('cd folder_name')
        """
        cmd = cmd.strip('\n')
        self.stdin.write(cmd + '\n')
        self.stdin.write(self.finish_cmd + '\n')
        shin = self.stdin
        self.stdin.flush()

        shout, sherr = [], []
        exit_status = 0
        for line in self.stdout:
            if str(line).startswith(cmd) or str(line).startswith(self.finish_cmd):
                # up for now filled with shell junk from stdin
                shout = []
            elif str(line).startswith(self.finish_str):
                # our finish command ends with the exit status
                exit_status = int(str(line).rsplit(maxsplit=1)[1])
                if exit_status:
                    # stderr is combined with stdout.
                    # thus, swap sherr with shout in a case of failure.
                    sherr = shout
                    shout = []
                break
            else:
                # get rid of 'coloring and formatting' special characters
                shout.append(re.compile(r'(\x9B|\x1B\[)[0-?]*[ -/]*[@-~]').
                                sub('', line).
                                replace('\b', '').
                                replace('\r', ''))

        # first and last lines of shout/sherr contain a prompt
        for shfile in [shout, sherr]:
            for prop in [{'cmd': cmd, 'num': 0}, 
                         {'cmd': self.finish_cmd, 'num': -1}]:
                if shfile and prop['cmd'] in shfile[prop['num']]:
                    shfile.pop(prop['num'])

        return shin, shout, sherr


class KernelParser:

    def __init__(self, prefix_kmod, prefix_capemgr):
        re_datestamp ="^\[\s*\d+\.\d+\]\s+"
        re_line = ":\s+(?P<line>.*)$"
        self.re_capemgr = re.compile(re_datestamp + re.escape(prefix_capemgr) +
            re_line, re.DOTALL)
        self.re_kmod = re.compile(re_datestamp + re.escape(prefix_kmod) +
            re_line, re.DOTALL)

    def parse_capemgr(self, list_str):
        list_output = []
        for str in list_str:
            output = self.re_capemgr.match(str)
            if output:
                output = output.groupdict().get('line', 'NONE')
                if output != "NONE":
                    list_output.append(output)

        return list_output

    def parse_kmod(self, list_str):
        list_output = []
        for str in list_str:
            output = self.re_kmod.match(str)
            if output:
                output = output.groupdict().get('line', 'NONE')
                if output != "NONE":
                    list_output.append(output)

        return list_output

def make_path(private_rsa_key):
    path = os.path.join(os.environ['HOME'], '.ssh', private_rsa_key)
    return path



# now, connect and use paramiko Client to negotiate SSH2 across the connection
if __name__ == "__main__":
    try:
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        print('--- Connecting...')
        client.connect(hostname, username='root', password=password,
                key_filename=make_path(private_rsa_key))
        transport = client.get_transport()
        handler = ShellHandler(client)
        handler.connect()
        stdin, stdout, stderr = handler.execute('uname -r')
        _, stdout, _ = handler.execute('load-overlays; dmesg | tail -n 10')

        parser = KernelParser("spi-protocol-generic",
            "bone_capemgr bone_capemgr")

        #stdout = parser.parse_capemgr(stdout)
        data = ""
        for line in stdout:
            data += line

        print(data)
        client.close()

    except Exception as e:
        print('*** Caught exception: %s: %s' % (e.__class__, e))
        traceback.print_exc()
        try:
            client.close()
        except:
            pass
        sys.exit(1)
