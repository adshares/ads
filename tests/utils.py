import json
import os
import subprocess
import ed25519

from tests.consts import (ESC_BIN_PATH, ACCOUNT_FIELDS, TRANSACTION_FIELDS,
                          TX_FIELDS, BROADCAST_FIELDS, BLOCK_FIELDS, BLOCK_NODE_FIELDS)


def exec_esc_cmd(client_id, js_command, with_get_me=True, cmd_extra=None, timeout=10):
    from .client.utils import get_client_dir
    client_dir = get_client_dir(client_id)

    esc_cmd = [ESC_BIN_PATH]
    if cmd_extra:
        esc_cmd.extend(cmd_extra)

    cmds = [js_command]
    if with_get_me:
        cmds.insert(0, {'run': "get_me"})

    process = subprocess.Popen(esc_cmd, cwd=client_dir, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, stdin=subprocess.PIPE)

    for cmd in cmds:
        print(str.encode(json.dumps(cmd) + "\n"))
        process.stdin.write(str.encode(json.dumps(cmd) + "\n"))

    stdout, stderr = process.communicate(timeout=timeout)

    raw_response = stdout.decode("utf-8")
    raw_response = raw_response.replace("}\n", "}").replace("}{", "},{")

    responses = json.loads("[%s]" %raw_response)

    return responses[-1]


def generate_keys():
    secret, pub_key = ed25519.create_keypair(entropy=os.urandom)

    new_secret_b = secret.to_ascii(encoding='hex')
    new_pub_key_b = pub_key.to_ascii(encoding='hex')
    signature = ed25519.to_ascii(secret.sign(b''), encoding='hex').decode('utf-8').upper()

    new_secret = new_secret_b.decode('utf-8').upper()
    new_pub_key = new_pub_key_b.decode('utf-8').upper()

    return new_secret, new_pub_key, signature


class ValidateObject(object):
    def __init__(self, obj, kind='account', structure=None):
        self.obj = obj
        self.structure = structure
        self.kind = kind
        self.type_objects = {
            'account': ACCOUNT_FIELDS,
            'transaction': TRANSACTION_FIELDS,
            'tx': TX_FIELDS,
            'broadcast': BROADCAST_FIELDS,
            'block': BLOCK_FIELDS,
            'block_node': BLOCK_NODE_FIELDS,
        }
        self.fields = self._get_required_fields()

    def _get_required_fields(self):
        if self.structure:
            return self.structure
        fields = self.type_objects.get(self.kind)
        if not fields:
            raise ValueError("Not correct type '{}'".format(self.kind))
        return fields

    def validate(self):
        for field in self.fields:
            if not field in self.obj:
                raise KeyError("{} doesn't has field {}".format(self.kind, field),
                               "Response: {}".format(self.obj))




