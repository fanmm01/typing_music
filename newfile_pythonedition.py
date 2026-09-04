import time

with open('example.txt', 'r') as f:
	
	def writeline(time=1,tok=""):
			f.write(tok)
			for i in range(15-len(tok)):
				f.write(" ")
			f.write("||")
			for i in range(10000):
				f.write(" ")
			f.write("\n")
	
	pai = int(input())
	for i in range(15):
		f.write(" ")
	f.write("||")
	for i in range(3000):
		for j in range(pai):
			for k in range(3):
				f.write(" ")
				f.write("." if(j!=pai-1) else "¡" )
	f.write("\n")
	writeline(1,"{")
	writeline(3)
	for i in range(2):
		writeline(1,"[")
		writeline()
	for i in range(10):
		writeline(1,"(")
		writeline()
	writeline("}")
	f.close()

	

	
	
		