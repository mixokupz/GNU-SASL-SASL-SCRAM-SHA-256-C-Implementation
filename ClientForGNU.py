from scramp import ScramClient, ScramMechanism
import socket
import base64
user = 'ivan'
pas = '12345'
mech = ['SCRAM-SHA-256']

host = "127.0.0.1"
port = 9999
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((host,port))



m = ScramMechanism()
print('')
c = ScramClient(mech,user,pas)
cfirst = c.get_client_first()
string64_cfirst = base64.b64encode(cfirst.encode("ascii"))
print(f"Client First:  {string64_cfirst}")

s.sendall(string64_cfirst)
data64 = s.recv(4096)

print(f"Recive:{data64}")

data = base64.b64decode(data64).decode("ascii")
c.set_server_first(data)

cfinal = c.get_client_final()

print(f"Client final: {cfinal}")
cfinal64 = base64.b64encode(cfinal.encode("ascii"))
s.sendall(cfinal64)
