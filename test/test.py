import base64
import getpass
import os
import socket
import sys
import traceback
import re
from paramiko.py3compat import input
import paramiko
import pprint
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
        re_datestamp = "^\[\s*\d+\.\d+\]\s+"
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

    def parse_capemgr_status(self, list_str):
        """
        parse output such this:

            Baseboard: 'A335BNLT,00C0,0816BBBK07B0'
            compatible-baseboard=ti,beaglebone-black - #slots=4
            slot #0: No cape found
            slot #1: No cape found
            slot #2: No cape found
            slot #3: No cape found
            enabled_partno PARTNO 'BB-ARDUINO-ECHO' VER 'N/A' PR '0'
            slot #4: override
            Using override eeprom data at slot 4
            slot #4: 'Override Board Name,00A0,Override Manuf,BB-ARDUINO-ECHO'
            initialized OK.
            slot #4: dtbo 'BB-ARDUINO-ECHO-00A0.dtbo' loaded; overlay id #0

            maybe that:
            loader: failed to load slot-4

        :param list_str: strings from executing remote command

        """
        slot_numbers = {}
        part = ""
        dict_output = {}

        re_slot_status = re.compile("^\s*slot\s*#(\d+):\ '*(.+?)'*$")
        re_enab_partno = re.compile(r"""
                                        ^\s*enabled_partno\s+
                                        PARTNO\ '(?P<part>.+?)'\s+
                                        VER\    '(?P<vers>.+?)'\s+
                                        PR\     '(?P<prio>.+?)'$
                                     """, re.VERBOSE)
        re_slot = re.compile("^[a-zA-Z\ ]+(\d+)$")
        re_status = re.compile(r"""
                                    ^[a-zA-Z\:\ ]*?
                                    (?P<status>failed|initialized\ OK)
                                    [a-zA-Z\ \-\.]+?
                                    (
                                        (?P<slot_num>\d+)
                                        \ (?P<slot_nam>[\-A-Za-z]+)\:
                                        (?P<slot_ver>[0-9a-eA-E]*)
                                        \ \(prio\ (?P<slot_pri>\d+)\)
                                    )*$
                                """, re.VERBOSE)

        list_str_body = list_str[6:]
        list_iter = iter(list_str_body)

        for line in list_iter:

            print(line)
            match = re_status.match(line)
            pp = pprint.PrettyPrinter(width=160)
            pp.pprint(match)
            pp.pprint(dict_output)
            if match and match.groupdict()['status'] == "failed":
                print("OK\n")
                num = match.groupdict()['slot_num']
                del dict_output['slot_info'][num]
                for key in dict_output['enabled_partno']:
                    if dict_output['enabled_partno'][key]['slot'] == num:
                        del dict_output['enabled_partno'][key]
                        break
                continue

            slot_line = next(list_iter)

            match = re_slot_status.match(slot_line)
            if match:
                slot_numbers[match.group(1)] = match.group(1)
                try:
                    dict_output['slot_info']
                except KeyError as e:
                    dict_output['slot_info'] = {}

                try:
                    dict_output['slot_info'][str(match.group(1))]
                except KeyError as e:
                    dict_output['slot_info'][str(match.group(1))] = []
                dict_output['slot_info'][str(match.group(1))].append(
                    match.group(2))

            match = re_enab_partno.match(line)
            if match:
                capture = match.groupdict()
                part = capture['part'];
                dict_output = self.check_dict(dict_output, part)
                dict_output['enabled_partno'][part]['ver'] = capture['vers']
                dict_output['enabled_partno'][part]['pr'] = capture['prio']
                continue

            match = re_slot.match(line)
            if match and match.group(1) == slot_numbers[match.group(1)]:
                dict_output = self.check_dict(dict_output, part)
                dict_output['enabled_partno'][part]['slot'] = slot_numbers[match.group(1)]
                continue

        return dict_output

    def parse_kmod(self, list_str):
        list_output = []
        for str in list_str:
            output = self.re_kmod.match(str)
            if output:
                output = output.groupdict().get('line', 'NONE')
                if output != "NONE":
                    list_output.append(output)

        return list_output

    def check_dict(self, dict, part):
        try:
            dict['enabled_partno']
        except KeyError as e:
            dict['enabled_partno'] = {}

        try:
            dict['enabled_partno'][part]
        except KeyError as e:
            dict['enabled_partno'][part] = {}

        return dict

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
        _, stdout, _ = handler.execute('load-overlays')

        parser = KernelParser("spi-protocol-generic",
            "bone_capemgr bone_capemgr")

        stdout = parser.parse_capemgr(stdout)
        stdout = parser.parse_capemgr_status(stdout)
        data = ""
#        for key in stdout:
#            data += stdout[key][0] + ' '
#            data += stdout[key][1] + '\n'

        pp = pprint.PrettyPrinter(indent=4, width=160)
        pp.pprint(stdout)
#        print(data)
        client.close()

    except Exception as e:
        print('*** Caught exception: %s: %s' % (e.__class__, e))
        traceback.print_exc()
        try:
            client.close()
        except:
            pass
        sys.exit(1)
