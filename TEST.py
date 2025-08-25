from typing import List,Tuple,Optional
from dataclasses import dataclass


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
			x = bytes.fromhex(res).decode("utf-8")

		except:
			x = res

		
		print(x,tlv.option,tlv.sub_option)


def examin():
	file:str
	bytes_=[]

	with open("bin/win/log.txt","r") as f:
		file = f.readlines()
	bit:str=""
	i = 0
	for line in file:
		if i >1 :
			break
		bytes_ = line.split(" ")
		new_(bytes_)
		print()
		i+=1
		
if __name__=="__main__":
	examin()

	