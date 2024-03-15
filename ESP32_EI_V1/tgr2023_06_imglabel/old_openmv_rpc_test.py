import json, rpc, serial, serial.tools.list_ports, struct, sys
from datetime import datetime
import time

rpc_master = rpc.rpc_usb_vcp_master('COM7')
result = rpc_master.call("jpeg_image_snapshot", recv_timeout=10000)

if result is not None:
    print(f"dir(result): {dir(result)}")

    # Check if the result has a 'nbytes' attribute before accessing it
    if hasattr(result, 'nbytes'):
        print(f"result.nbytes: {result.nbytes}")
        jpg_sz = int.from_bytes(result.tobytes(), "little")
        print(f"jpg_sz: {jpg_sz}")

        buf = bytearray(b'\x00'*jpg_sz)
        result = rpc_master.call("jpeg_image_read", recv_timeout=10000)
        
        if result is not None:
            rpc_master.get_bytes(buf, jpg_sz)
            print(f"len(buf): {len(buf)}")
            
            with open("test.jpg", "wb") as f:
                f.write(buf)
        else:
            print("Error: jpeg_image_read returned None.")
    else:
        print("Error: 'nbytes' attribute not found in result.")
else:
    print("Error: jpeg_image_snapshot returned None.")
