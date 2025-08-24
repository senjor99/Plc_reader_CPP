from typing import List,Tuple,Optional
from dataclasses import dataclass
req={}
def parse_hex_line(line: str):

    toks = [t.strip().upper() for t in line.split() if len(t.strip()) == 2]

    toks = [t for t in toks if all(c in "0123456789ABCDEF" for c in t)]
    return toks

def be16(hi, lo):
    return (int(hi,16)<<8) | int(lo,16)

def mac_str(toks, off):
    return ":".join(toks[off+ i] for i in range(6))

def analyze_line(line, xid_hex=("8A","B4","83","20")):
    b = parse_hex_line(line)
    if len(b) < 30:
        return  # troppo corto per DCP

    if not (b[12] == "88" and b[13] == "92"):
        return
    
    xid_match = (b[18], b[19], b[20], b[21]) == xid_hex
    if not xid_match:
        return

    data_len = be16(b[24], b[25])
    tlv_start = 26
    tlv_end   = tlv_start + data_len
    if tlv_end > len(b):
        # safety: tronca al buffer reale
        tlv_end = len(b)

    print("SRC MAC:", mac_str(b, 6))
    idx = tlv_start
    while idx + 4 <= tlv_end:
        opt = b[idx]
        sub = b[idx+1]
        blen = be16(b[idx+2], b[idx+3])
        idx += 4

        # Limite del value
        val_end = idx + blen
        if val_end > tlv_end:
            # blocco malformato: esci o tronca
            val_end = tlv_end

        # Raw del Value senza spazi
        val_raw = "".join(b[idx:val_end])
		
        print(f"  BLOCK opt={opt} sub={sub} len={blen} raw={val_raw}")

        # avanza oltre il value
        idx = val_end
        # padding: se blen è dispari, salta 1 byte (se presente)
        if (blen % 2) == 1 and idx < tlv_end:
            idx += 1

def analize(byte_array:list): 
	xid = ("8A","B4","83","20")
	if ((byte_array[18],byte_array[19] ,byte_array[20],byte_array[21]) == xid): 
		internal_idx = 26 
		TLV = "" 
		print("MAC EMITTER: ",byte_array[6],byte_array[7],byte_array[8],byte_array[9],byte_array[10],byte_array[11],": ") 
		while internal_idx + 4 <= len(byte_array):
			option = byte_array[internal_idx] #option 
			sub =byte_array[internal_idx+1] #sub-option 

			if sub != "02" or option != "02":
				internal_idx +=4+TLV_len 
				break
			else:
			
				TLV_len = int(byte_array[internal_idx+2]+byte_array[internal_idx+3],16) #datalen 
				if TLV_len > 2:
					print("OPTION:",option,"SUB-OPTION:",sub,"TLV LEN:",TLV_len,end=" ") 
					for i in range(internal_idx+4,internal_idx+4+TLV_len): 
						TLV +=byte_array[i] 
                        
				internal_idx +=4+TLV_len 
				print("TLV: ",TLV) 
	else: 
		return 
	
@dataclass
class TLV:
	option:str
	sub_option:str
	length:int
	bytes_:List[str]
	body:Optional[str] = ""
	to_pop:Optional[Tuple[int,int]] =()

	@classmethod
	def from_byte(cls,bt):
		length =  int("".join([bt[2],bt[3]]),16)
		instance = cls(
			option = bt[0],
			sub_option = bt[1],
			length = length ,
			bytes_=bt
			)

		return instance
	
	def process(self)->str:
		self.body= "".join([self.bytes_[t] for t in range(4,4+self.length)])
		return self.body


def new_(bytes_:list):
	
	xid = ("8A","B4","83","20")

	tlvs_len=int(bytes_[24]+bytes_[25],16)
	starting_block=abs(len(bytes_)-tlvs_len)
	
	if(starting_block!=26):
		return

	tlvs =[bytes_[i] for i in range(starting_block,starting_block+tlvs_len)]

	while len(tlvs)>1:
		tlv =TLV.from_byte(tlvs)
		res=tlv.process()
		padding = 0 if tlv.length %2 == 0 else 1
		for i in range(0,4+tlv.length+padding):
			tlvs.pop(0)
		try:
			res = bytes.fromhex(res).decode("utf-8")
		except:
			pass

		if("plc".lower() in res.lower()):
			print(res)


def examin():
	file:str
	bytes_=[]

	with open("plc_reader_CPP\\bin\\win\\log.txt","r") as f:
		file = f.readlines()
	bit:str=""
	for line in file:
		bytes_ = line.split(" ")
		new_(bytes_)
		
		#analize(line)
		#bytes_.clear()
	#for i in req.keys():
		#print(f"Req : {i} done {req[i]} times")
if __name__=="__main__":
	examin()

	