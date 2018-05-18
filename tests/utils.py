import json
import subprocess

from .consts import ESC_BIN_PATH


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
        process.stdin.write(str.encode(json.dumps(cmd) + "\n"))

    stdout, stderr = process.communicate(timeout=timeout)

    raw_response = stdout.decode("utf-8")
    raw_response = raw_response.replace("}\n", "}").replace("}{", "},{")

    responses = json.loads("[%s]" %raw_response)

    return responses[-1]