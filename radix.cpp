/*  Stream base/radix conversion
    This takes a data stream and converts the radix from x to y. Radices can be anything from 2 up to about 4294967296...
    Copyright (C) 2017  Justin Swatsenbarg

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/* Note: This can be rather easily modified to support practically any radix to infinity by using a *good* big int library that supports log (yes! It isn't really that hard) and returns the count of bits used as a big int number. */

// TODO: Testing various options... currently on -S "FEDCBA9876543210"
// -A broken

#include <streambuf>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cctype>

#define SIZE_CHANGE(L, F, T) (ceil(L * log10(F) / log10(T)))
#define MIN(x, y) (x < y ? x : y)
#define OPTION(a) (0x2D00 | (unsigned char)(a))

using namespace std;

class base : public basic_streambuf<char>
{
public:
	enum action { DROP, ZERO, BREAK, REPORT, EXIT };
private:
	bool in_case, upper_case;
	enum action action_invalid;
	uint16_t _chunk_size, _reads, _writes;
	uint32_t _from, _to;
	vector<uint32_t> buffer;
	unordered_map<uint32_t, uint32_t> char2index, /* char represenation -> index value */ index2char; /* index value -> char represenation */
	void init(uint32_t, uint32_t);
	uint32_t forcecase(uint32_t c) { return upper_case ? toupper(c) : tolower(c); }
	unsigned char standardcase(unsigned char c) { return in_case ? toupper(c) : tolower(c); }
	void convert(vector<uint32_t>, vector<uint32_t> &);
	void negotiatebase(uint32_t, uint32_t);
public:
	base(uint32_t f, uint32_t t) { init(f, t); }
	void setchar2index(string);
	void setindex2char(string);

	base &operator <<(uint64_t);
	base &operator <<(vector<uint8_t>);
	base &operator <<(vector<uint16_t>);
	base &operator <<(vector<uint32_t>);
	base &operator <<(vector<uint64_t>);
	base &operator <<(string);

	base &operator >>(uint64_t &);
	base &operator >>(vector<uint8_t> &);
	base &operator >>(vector<uint16_t> &);
	base &operator >>(vector<uint32_t> &);
	base &operator >>(vector<uint64_t> &);
	base &operator >>(string &);

	base &operator = (vector<uint8_t> &);
	base &operator = (vector<uint16_t> &);
	base &operator = (vector<uint32_t> &);
	base &operator = (vector<uint64_t> &);
	base &operator = (string);

	operator vector<uint8_t> ();
	operator vector<uint16_t> ();
	operator vector<uint32_t> ();
	operator vector<uint64_t> ();
	operator string();

	uint32_t frombase(uint32_t f = 0) { if (f > 1) negotiatebase(_from = f, _to); return _from; }
	uint32_t tobase(uint32_t t = 0) { if (t > 1) negotiatebase(_from, _to = t); return _to; }

	uint32_t readsize() { return _reads; }
	uint32_t writesize() { return _writes; }

	void inlowercase() { in_case = false; }
	void inuppercase() { in_case = true; }
	char zero() { return index2char[0]; } // For fill cases where zero is not the character '0'

	friend ostream &operator <<(ostream &, base &);
	friend istream &operator >>(istream &, base &);

	void setinvalid(action a) { action_invalid = a; }
	void setuppercase() { upper_case = true; in_case = false; }
	void setlowercase() { upper_case = in_case = false; }
#ifdef DEBUG
	void test();
#endif
};

void base::init(uint32_t f, uint32_t t)
{
	uint32_t i;
	string bl = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	in_case = t > 26;
	upper_case = false;
	negotiatebase(f, t);
	index2char.reserve(sizeof(bl));
	char2index.reserve(sizeof(bl));
	for (i = 0; i < bl.size(); i++) index2char[char2index[standardcase(bl[i])] = (uint32_t)i] = standardcase(bl[i]);
}

void base::negotiatebase(uint32_t f, uint32_t t)
{
	_chunk_size = (uint16_t)SIZE_CHANGE(1, _from = f, _to = t);
	if (_from < _to) { _reads = _chunk_size; _writes = 1; }
	else { _writes = _chunk_size; _reads = 1; }
	if(buffer.size() < (1 + (buffer.size() / _chunk_size))) buffer.resize(1 + (buffer.size() / _chunk_size)); // Round size up to nearest multiple of _chunk_size
}

void base::setchar2index(string c2i)
{
    char2index.clear();
    for (uint32_t i = 0; i < c2i.size(); i++)
		char2index[(unsigned char)standardcase(c2i[(const unsigned int)standardcase(i)])] = (uint32_t)i;
    
}
void base::setindex2char(string i2c)
{
	index2char.clear();
	for (uint32_t i = 0; i < i2c.size(); i++)
		index2char[i] = standardcase(i2c[(const unsigned int)standardcase(i)]);
}

base &base::operator <<(uint64_t u) { vector<uint32_t> x(1); x[0] = u; buffer.resize(SIZE_CHANGE(x.size(), _from, _to)); convert(x, buffer); return *this; }
base &base::operator <<(vector<uint8_t> v)
{
	vector<uint32_t> in(v.begin(), v.end());
	convert(in, buffer);
	return *this;
}
base &base::operator <<(vector<uint16_t> v)
{
	vector<uint32_t> in(v.begin(), v.end());
	convert(in, buffer);
	return *this;
}
base &base::operator <<(vector<uint32_t> v) { convert(v, buffer); return *this; }
base &base::operator <<(vector<uint64_t> v)
{
	vector<uint32_t> in(v.begin(), v.end());
	convert(in, buffer);
	return *this;
}
base &base::operator <<(string s)
{
	vector<uint32_t> in(s.size());
	for (uint32_t i = 0; i < s.size(); i++)
		try { in[i] = char2index.at(standardcase(s[i])); }
		catch(out_of_range)
		{
			switch(action_invalid)
			{
				case ZERO:
					in[i] = 0;
					break;
				case BREAK:
					return *this;
				case REPORT:
					cerr << "Invalid value: '" << s[i] << "'" << endl;
					break;
				case EXIT:
					exit(1);
				case DROP:
				default:
					break;
			}
		}
	convert(in, buffer);
	return *this;
}

base &base::operator >>(uint64_t &u) { u = buffer.front(); buffer.erase(buffer.begin(), buffer.begin() + 1);  return *this; }
base &base::operator >>(vector<uint8_t> &v) { v.assign(buffer.begin(), buffer.end()); return *this; }
base &base::operator >>(vector<uint16_t> &v) { v.assign(buffer.begin(), buffer.end()); return *this; }
base &base::operator >>(vector<uint32_t> &v) { v.assign(buffer.begin(), buffer.end()); return *this; }
base &base::operator >>(vector<uint64_t> &v) { v.assign(buffer.begin(), buffer.end()); return *this; }
base &base::operator >>(string &s)
{
	s.clear();
	s.resize(buffer.size());
	for (uint32_t i = 0; i < buffer.size(); i++) s[i] = forcecase(index2char[buffer[i]]);
	return *this;
}

base &base::operator = (vector<uint8_t> &v)
{
	vector<uint32_t> in(v.begin(), v.end());
	buffer.clear();
	convert(in, buffer);
	return *this;
}
base &base::operator = (vector<uint16_t> &v)
{
	vector<uint32_t> in(v.begin(), v.end());
	buffer.clear();
	convert(in, buffer);
	return *this;
}
base &base::operator = (vector<uint32_t> &v)
{
	vector<uint32_t> in(v.begin(), v.end());
	buffer.clear();
	convert(in, buffer);
	return *this;
}
base &base::operator = (vector<uint64_t> &v)
{
	vector<uint32_t> in(v.begin(), v.end());
	buffer.clear();
	convert(in, buffer);
	return *this;
}
base &base::operator = (const string s)
{
	vector<uint32_t> in(s.begin(), s.end());
	buffer.clear();
	convert(in, buffer);
	return *this;
}

base::operator vector<uint8_t>() { vector<uint8_t> b(buffer.begin(), buffer.end()); return b; }
base::operator vector<uint16_t>() { vector<uint16_t> b(buffer.begin(), buffer.end()); return b; }
base::operator vector<uint32_t>() { vector<uint32_t> b(buffer.begin(), buffer.end()); return b; }
base::operator vector<uint64_t>() { vector<uint64_t> b(buffer.begin(), buffer.end()); return b; }
base::operator string() { string s(buffer.begin(), buffer.end()); return s; } // Raw index string

ostream& operator <<(ostream &os, base &val)
{
	for (vector<uint32_t>::const_iterator i = val.buffer.begin(); i < val.buffer.end(); ++i) os << (char)val.index2char[*i];
	os << endl;
	return os;
}
istream& operator >>(istream &is, base &se)
{
	vector<uint32_t> in;
	string tmp;
	is >> tmp;
	in.reserve(tmp.size());
	for (string::const_iterator i = tmp.begin(); i < tmp.end(); ++i) in.push_back(se.char2index.at(*i));
	se.convert(in, se.buffer);
	return is;
}

void base::convert(vector<uint32_t> in, vector<uint32_t> &out)
{
	int64_t a, i, t, x, z;
	out.clear();
	out.resize((uint32_t)SIZE_CHANGE(in.size(), _from, _to));
	while (in.size() > 1 && in[0] == 0) in.erase(in.begin(), in.begin() + 1); // Remove leading zeros; Alt: in.erase(in.begin(), in.begin() + min(in.find_first_not_of('0'), in.size() - 1));
	for (i = in.size() - 1, x = 1; i >= 0; i--, x *= _from)
		for (a = x * in[i], t = out.size() - 1; a > 0 && t >= 0; a += out[t], out[t--] = a % _to, a /= _to);
	while (out.size() > 1 && out[0] == 0) out.erase(out.begin(), out.begin() + 1); // Remove leading zeros; Alt: out.erase(out.begin(), out.begin() + min(out.find_first_not_of('\0'), out.size() - 1));
}

#ifdef DEBUG
void base::test()
{
	string out;
	frombase(16);
	tobase(2);
	cout << "From base: " << frombase() << endl;
	cout << "To base: " << tobase() << endl;
	cout << endl;

	uint8_t u = 0xF;
	this->operator << (u);
	this->operator >> (out);
	cout << "Char result: " << std::hex << (uint16_t)u << std::dec << " -> " << out << endl;

	uint32_t ui[4] = { 1, 0, 0, 0 };
	vector<uint32_t> vui(ui, ui + sizeof(ui) / sizeof(ui[0]));
	this->operator << (vui);
	this->operator >> (out);
	cout << "Int vector result: " << vui[0];
	for(uint16_t i = 1; i < vui.size(); i++) cout << ", " << vui[i];
	cout << " -> " << out << endl;

	string s = "F00";
	this->operator << (s);
	this->operator >> (out);
	cout << "String result: " << '_' << frombase() << ':' << s << " -> " << '_' << tobase() << ':' << out << endl;
	cout << "Testing complete!\n";
}
#endif
int printHelp()
{
	cout <<
		"Stream Arbitrary Base Converter  Copyright (C) 2017  Justin Swatsenbarg\n"
		"This takes a data stream and converts the radix from x to y. Radices can be anything from 2 up to about 4294967296...\n\n"
		"This program comes with ABSOLUTELY NO WARRANTY!\n"
		"This is free software, and you are welcome to redistribute\n"
		"it under certain conditions of the GPL v2 license.\n"
		"See <http://www.gnu.org/licenses/> for details.\n"
		"	-f=x from base x\n"
		"	-F=x to base y\n"
		"	-s=\"012345...\" from base string\n"
		"	-S=\"012345...\" to base string\n"
		"Shortcuts\n"
		" From base\n"
		"	-b binary (base 2)\n"
		"	-o octal (base 8)\n"
		"	-d decimal (base 10)\n"
		"	-x hexadecimal (base 16)\n"
		"	-t hexatrigesimal (base 36)\n"

		"	-l lowercase\n"
		"	-u uppercase\n"
		" To base\n"
		"	-B binary (base 2)\n"
		"	-O octal (base 8)\n"
		"	-D decimal (base 10)\n"
		"	-X hexadecimal (base 16)\n"
		"	-T hexatrigesimal (base 36)\n"

        "	-L lowercase\n"
		"	-U uppercase\n"
		"Combination shortcut\n"
		"	-a ascii input base 256 string\n"
		"	-A ascii output base 256 string\n"
		"Action on invalid character (mutually exclusive)\n"
		"	-p drop\n"
		"	-z zero\n"
		"	-k break read \n"
		"	-r report to standard error\n"
		"	-e exit with error level\n"
#ifdef DEBUG
		"	-R run tests\n"
#endif
		"	-h help (this screen)\n";
	return 0;
}

int main(int argc, char *argv[])
{
	bool asciiout = false;
	int32_t r, _;
	uint32_t f, t;
	string s;
	if(argc == 1) return printHelp();
	base b(10, 2);
	while(--argc > 0)
	{
		s.assign(argv[argc]);
		switch(s[0] << 8 | s[1])
		{
			case OPTION('b'): // From binary
				b.frombase(2);
				b.setchar2index("01");
				break;
			case OPTION('o'): // From octal
				b.frombase(8);
				b.setchar2index("01234567");
				break;
			case OPTION('d'): // From decimal
				b.frombase(10);
				b.setchar2index("0123456789");
				break;
			case OPTION('x'): // From hexadecimal
				b.frombase(16);
				b.setchar2index("0123456789abcdef");
				break;
			case OPTION('t'): // From hexatrigesimal (base 36)
				b.frombase(36);
				b.setchar2index("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
				break;
			case OPTION('B'): // To binary
				b.tobase(2);
				b.setindex2char("01");
				break;
			case OPTION('O'): // To octal
				b.tobase(8);
				b.setindex2char("01234567");
				break;
			case OPTION('D'): // To decimal
				b.tobase(10);
				b.setindex2char("0123456789");
				break;
			case OPTION('X'): // To hexadecimal
				b.tobase(16);
				b.setindex2char("0123456789abcdef");
				break;
			case OPTION('T'): // To hexatrigesimal (base 36)
				b.tobase(36);
				b.setindex2char("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
				break;
			case OPTION('f'): // Set from base
				istringstream(s.substr(3)) >> f;
				b.frombase(f);
				break;
			case OPTION('F'): // Set to base
				istringstream(s.substr(3)) >> t;
				b.tobase(t);
				break;
			case OPTION('s'): // Set from string
				b.setchar2index(s.substr(s.find_first_of('=') + 1, s.length() - 1));
				break;
			case OPTION('S'): // Set to string
				b.setindex2char(s.substr(s.find_first_of('=') + 1, s.length() - 1));
				break;
			case OPTION('l'): // From lowercase
				b.setlowercase();
				break;
			case OPTION('u'): // From uppercase
				b.setuppercase();
				break;
			case OPTION('L'): // To lowercase
				b.inlowercase();
				break;
			case OPTION('U'): // To uppercase
				b.inuppercase();
				break;
			case OPTION('a'): // Ascii/binary input mode
				s.resize(256);
				for(_ = 0; _ < 256; _++) s[_] = (char)_;
				b.setchar2index(s);
				b.frombase(256);
				break;
			case OPTION('A'): // Ascii/binary output mode
				asciiout = true;
				s.resize(256);
				for(_ = 0; _ < 256; _++) s[_] = (char)_;
				b.setindex2char(s);
				b.tobase(256);
				break;
			case OPTION('p'): // Drop invalid characters in input (default)
				b.setinvalid(base::DROP);
				break;
			case OPTION('z'): // Zero invalid characters in input
				b.setinvalid(base::ZERO);
				break;
			case OPTION('k'): // Break read on invalid characters in input
				b.setinvalid(base::BREAK);
				break;
			case OPTION('r'): // Report invalid characters in input
				b.setinvalid(base::REPORT);
				break;
			case OPTION('e'): // Exit (with error level) on invalid characters in input
				b.setinvalid(base::EXIT);
				break;
#ifdef DEBUG
			case OPTION('R'): // Run tests
				b.test();
				return 0;
#endif
			case OPTION('h'): // Help
			default:
				return printHelp();
		}
	}

	string data(b.readsize(), '\0');
	for(cin.read(&data[0], b.readsize()); r = cin.gcount(); cin.clear(), cin.read(&data[0], b.readsize()))
	{
		data.resize(r);
		b << data;
		b >> data;
		for(_ = b.writesize() - data.size(); _ > 0; _--) cout << b.zero(); // Zero pad
		cout.write(&data[0], MIN(data.size(), b.writesize()));
	}
	if(asciiout) cout << flush << flush; else cout << endl << endl;
}
