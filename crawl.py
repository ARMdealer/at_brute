import requests,re
from bs4 import BeautifulSoup

#global vars
url = "https://atcommands.org/atdb/image/1578?page="

def get_text(html, outfile):
	out = open(outfile, 'a')
	#res = []
	soup = BeautifulSoup(html,"html.parser")
	lines = soup.find_all("a")
	for line in lines:
		line = line.text
		if line.startswith('AT'):
			#res.append(line.strip('\r\n'))
			out.write(line+'='+'\n')

	out.close()

	#return res


def main():
	for i in xrange(1,22):
		r = requests.get(url+"%d"%i)
		html = r.text
		get_text(html, "N950FXXU1AQI1_N950FOXM1AQHF_BTU_7.1.1.txt")
	
			

if __name__ == "__main__":
	main()
