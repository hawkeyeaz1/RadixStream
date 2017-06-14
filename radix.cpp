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
// NOTE: This is NOT directly compatible with most base conversion methods as it is a stream converter and cannot use the whole number!
/* Note: This can be rather easily modified to support practically any radix to infinity by using a big int library that returns the count of bits (i.e. log2) used as a big int number. */

//#define DEBUG

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

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x < y ? y : x)
#define SIZE_CHANGE(F, T) (((FILog2(MAX(F, T)) << 1) / FILog2(MIN(F, T))) >> 1)

#define WC2UI16(c) ((*(uint16_t *)&c))
#if('ab' == (('a' << 8) | 'b'))
    #define ENDIAN(wb) ((uint16_t)((wb << 8) | (wb & wb & 0xFF00) >> 8))
#else
    #define ENDIAN(wb) ((uint16_t)(wb))
#endif

using namespace std;

/* Description: Converts between radices (bases) between 2 and 4294967297 as a stream */

class base : public basic_streambuf<char>
{
public:
	enum action { IGNORE, ZERO, QUIT, INFORM, EXIT, THROW };
private:
	bool in_case, upper_case;
	enum action action_invalid;
	char FILogTab256[256];
	uint16_t _chunk_size, _reads, _writes;
	uint32_t _from, _to;
	vector<uint32_t> buffer;
	unordered_map<uint32_t, uint32_t> char2index, index2char;
	uint32_t forcecase(uint32_t c) { return upper_case ? toupper(c) : tolower(c); }
	uint32_t FILog2(uint64_t);
	unsigned char standardcase(unsigned char c) { return in_case ? toupper(c) : tolower(c); }
	string bl;
	void convert(vector<uint32_t>, vector<uint32_t> &);
	void negotiatebase(uint32_t, uint32_t);
public:
	base(uint32_t, uint32_t, const string &, const string &, bool, bool, action);
	/* Description: Sets map Char2Index from c2i string
	 * Parameters:
	 *	c2i: String of characters to use for the input character to number conversion
	 */
	void setchar2index(string c2i) { char2index.clear(); for (uint32_t i = 0; i < c2i.size(); i++) char2index[standardcase(c2i[standardcase(i)])] = (uint32_t)i; }
	/* Description: Sets map Index2Char from i2c string
	 * Parameters:
	 *	i2c: String of characters to use for the output character to number conversion
	 */
	void setindex2char(string i2c) { index2char.clear(); for (uint32_t i = 0; i < i2c.size(); i++) index2char[i] = standardcase(i2c[standardcase(i)]); }
	void seti2cslice(uint16_t s, uint16_t e) { setchar2index(bl.substr(s, e)); }
	void setc2islice(uint16_t s, uint16_t e) { setindex2char(bl.substr(s, e)); }

	base &operator <<(uint64_t);
	base &operator <<(vector<uint8_t>);
	base &operator <<(vector<uint16_t>);
	base &operator <<(vector<uint32_t>);
	base &operator <<(vector<uint64_t>);
	base &operator <<(string);
	base &operator = (vector<uint8_t> &);
	base &operator = (vector<uint16_t> &);
	base &operator = (vector<uint32_t> &);
	base &operator = (vector<uint64_t> &);
	base &operator = (string);
	base &operator >>(uint64_t &);
	base &operator >>(vector<uint8_t> &);
	base &operator >>(vector<uint16_t> &);
	base &operator >>(vector<uint32_t> &);
	base &operator >>(vector<uint64_t> &);
	base &operator >>(string &);
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
	void setinvalid(action a) { action_invalid = a; }
	void setuppercase() { upper_case = true; in_case = false; }
	void setlowercase() { upper_case = in_case = false; }
#ifdef DEBUG
	static void test();
#endif
	friend ostream &operator <<(ostream &, base &);
	friend istream &operator >>(istream &, base &);
};

/* Parameters:
 *	f: From radix
 *	t: To radix
 *	fs: From string; must be a string of unique characters
 *	ts: To string; must be a string of unique characters
 *	inUpperCase: Force input case to uppercase (otherwise lowercase)
 *	outUpperCase: Force output case to uppercase (otherwise lowercase)
 *	todo: Action to take when an invalid character is encountered
 */
base::base(uint32_t f = 36, uint32_t t = 36, const string &fs = "", const string &ts = "", bool inUpperCase = true, bool outUpperCase = true, action todo = IGNORE)
{
	bl = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	string _fs, _ts;
	setinvalid(todo);
	in_case = inUpperCase;
	upper_case = outUpperCase;
	if(f < 2 || t < 2) throw "Invalid radix";
	if(fs.size() < f) _fs.assign(bl.substr(0, f)); else _fs.assign(fs);
	if(ts.size() < t) _ts.assign(bl.substr(0, t)); else _ts.assign(ts);
	if(f > _fs.size() || t > _ts.size()) throw "Radix value inconsistent with radix string!";
	for (uint32_t i = 0; i < bl.size(); i++) index2char[char2index[standardcase(bl[i])] = (uint32_t)i] = standardcase(bl[i]);
	FILogTab256[0] = FILogTab256[1] = 1; // We still need 1 character 'digit' for '0'
	for (uint16_t i = 2; i < sizeof(FILogTab256); i++) FILogTab256[i] = 1 + FILogTab256[i >> 1];
	negotiatebase(f, t);
	char2index.reserve(f);
	index2char.reserve(t);
}

/* Description: Sets related variables based on input, output radix ratios
 * Parameters:
 *	f: From radix
 * 	t: To radix
 */
void base::negotiatebase(uint32_t f, uint32_t t)
{
	_from = f;
	_to = t;
	_chunk_size = (uint16_t)SIZE_CHANGE(f, t); // We take max, min to ensure the ratio is not fractional
	if (_from < _to) { _reads = _chunk_size; _writes = 1; }
	else { _writes = _chunk_size; _reads = 1; }
	if(buffer.size() < (1 + (buffer.size() / _chunk_size))) buffer.resize(1 + (buffer.size() / _chunk_size)); // Round size up to nearest multiple of _chunk_size
}

/* Description: Modified log2 ("floor integer") for determining how many digits we need to reserve space for
 * Parameter: v: Value to get the log of
 */
uint32_t base::FILog2(uint64_t v)
{
	register uint8_t t;
	register uint16_t r;
	if (t = v >> 24) return 24 + FILogTab256[t]; 
	if (t = v >> 16) return 16 + FILogTab256[t]; 
	if (t = v >> 8) return 8 + FILogTab256[t];
	return FILogTab256[v];
}

#define OPLSFN vector<uint32_t> in(v.begin(), v.end()); convert(in, buffer); return *this
base &base::operator <<(uint64_t u) { vector<uint32_t> x(1); x[0] = u; buffer.resize(x.size() * SIZE_CHANGE(_from, _to)); convert(x, buffer); return *this; }
base &base::operator <<(vector<uint8_t> v) { OPLSFN; }
base &base::operator <<(vector<uint16_t> v) { OPLSFN; }
base &base::operator <<(vector<uint32_t> v) { convert(v, buffer); return *this; }
base &base::operator <<(vector<uint64_t> v) { OPLSFN; }
base &base::operator <<(string s)
{
	vector<uint32_t> in(s.size());
	for (uint32_t i = 0, o = 0; i < s.size(); i++)
	{
		try { in[i - o] = char2index.at(standardcase(s[i])); }
		catch(out_of_range)
		{
			switch(action_invalid)
			{
				case ZERO: in[i] = 0; break;
				case QUIT: return *this;
				case EXIT: exit(1);
				case THROW: throw out_of_range("Invalid index!");
				case INFORM: cerr << "Invalid value: '" << s[i] << "'" << endl;
				case IGNORE:
					o++; // Now the input does not track perfectly w/ the output--apply an offset
					in.resize(in.size() - 1); // Free up the extra digit
				default: break;
			}
		}
	}
	convert(in, buffer);
	return *this;
}

#define OPEQFN(var) vector<uint32_t> in((var).begin(), (var).end()); buffer.clear(); convert(in, buffer); return *this
base &base::operator = (vector<uint8_t> &v)  { OPEQFN(v); }
base &base::operator = (vector<uint16_t> &v) { OPEQFN(v); }
base &base::operator = (vector<uint32_t> &v) { OPEQFN(v); }
base &base::operator = (vector<uint64_t> &v) { OPEQFN(v); }
base &base::operator = (const string s) { OPEQFN(s); } // TODO: This needs to force case!!
#define OPRSFN v.assign(buffer.begin(), buffer.end()); return *this
base &base::operator >>(uint64_t &u) { u = buffer.front(); buffer.erase(buffer.begin(), buffer.begin() + 1);  return *this; }
base &base::operator >>(vector<uint8_t> &v)  { OPRSFN; }
base &base::operator >>(vector<uint16_t> &v) { OPRSFN; }
base &base::operator >>(vector<uint32_t> &v) { OPRSFN; }
base &base::operator >>(vector<uint64_t> &v) { OPRSFN; }
base &base::operator >>(string &s)
{
	s.clear();
	s.resize(buffer.size());
	for (uint32_t i = 0; i < buffer.size(); i++) s[i] = forcecase(index2char[buffer[i]]);
	return *this;
}
base::operator vector<uint8_t>()  { vector<uint8_t> b(buffer.begin(), buffer.end()); return b; }
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

/* Description: Converts number in to number out radix on from and to radix
 * Parameters:
 *	in: Vector where each array element is one digit in from radix
 *	out: Vector to place output where each array element is one digit in to radix
 */
void base::convert(vector<uint32_t> in, vector<uint32_t> &out)
{
	int64_t a, i, t, x, z;
	out.clear();
	out.resize(in.size() * (uint32_t)SIZE_CHANGE(_from, _to));
	while (in.size() > 1 && in[0] == 0) in.erase(in.begin(), in.begin() + 1); // Remove leading zeros; Alt: in.erase(in.begin(), in.begin() + min(in.find_first_not_of('0'), in.size() - 1));
	for (i = in.size() - 1, x = 1; i >= 0; i--, x *= _from)
		for (a = x * in[i], t = out.size() - 1; a > 0 && t >= 0; a += out[t], out[t--] = a % _to, a /= _to);
	while (out.size() > 1 && out[0] == 0) out.erase(out.begin(), out.begin() + 1); // Remove leading zeros; Alt: out.erase(out.begin(), out.begin() + min(out.find_first_not_of('\0'), out.size() - 1));
}

#ifdef DEBUG
void base::test()
{
	string out, s = "F00";
	base b(16, 2);
	cout << "From base: " << b.frombase() << endl << "To base: " << b.tobase() << endl << endl;

	uint8_t u = 0xF;
	b << u;
	b >> out;
	cout << "Char result: " << std::hex << (uint16_t)u << std::dec << " -> " << out << endl;

	uint32_t ui[4] = { 1, 0, 0, 0 };
	vector<uint32_t> vui(ui, ui + sizeof(ui) / sizeof(ui[0]));
	b << vui;
	b >> out;
	cout << "Int vector result: " << vui[0];
	for(uint16_t i = 1; i < vui.size(); i++) cout << ", " << vui[i];
	cout << " -> " << out << endl;

	b << s;
	b >> out;
	cout << "String result: " << '_' << b.frombase() << ':' << s << " -> " << '_' << b.tobase() << ':' << out << endl;
	cout << "Testing complete!\n";
}
#endif
int printHelp()
{
	cout <<
		"Stream Arbitrary Base Converter    Copyright (C) 2017  Justin Swatsenbarg\n"
		"This takes a data stream and converts the radix from x to y.\n"
		"Radices can be anything from 2 up to about 4294967296...\n\n"
		"This program comes with ABSOLUTELY NO WARRANTY!\n"
		"This is free software, and you are welcome to redistribute\n"
		"it under certain conditions of the GPL v2 license.\n"
		"See <http://www.gnu.org/licenses/> for details.\n"
		"\n"
		"Prefixing each option with '-' is optional\n"
		"From:                           To:\n"
		" fr[adix]                        tr[adix]\n"
		" fs[tring]=X                     ts[tring]=X\n"
		"    Default string is \"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\"\n"
		"Case:                     Note: From case is only useful with input radix string\n"
		" fl[owercase]                    tl[owercase]\n"
		" fu[ppercase]                    tu[ppercase]\n"
		"Shortcut\n"
		"  Equivalent to fr=v or tr=v respectively where v is the radix value\n"
		" f2, fb[inary]                   t2, tb[inary]\n"
		" f8, fo[ctal]                    t8, to[ctal]\n"
		" f1[0], fd[ecimal]               t1[0], td[ecimal]\n"
		" fx, fh[exidecimal]              tx, th[exidecimal]\n"
		" ft, f3[6], fH[exatrigesimal]    tt, t3[6], tH[exatrigesimal]\n"
		"Combo:\n"
		" fa[scii]                        ta[scii] Sets radix 256 and the respective string to ascii\n"
		"Invalid character action\n"
		" ig[nore], sk[ip]       Ignore (skip) and continue\n"
		" ze[ro]                 Zero invalid values\n"
		" qu[uit], st[op]        Quit reading input\n"
		" in[form], dr[op]       Inform on stderr, ignore and continue\n"
		" ex[it], er[ror]        Exit program with error level 1 (abnormal exit)\n"
		"\n"
#ifdef DEBUG
		" [-]te[st]                 Verify with tests\n"
#endif
		" -h, [-]he[lp]             Help\n"
		"\n";
	return 0;
}

int main(int argc, char *argv[])
{
	bool asciiout = false, inUpperCase, outUpperCase;
#ifdef DEBUG
	bool tests = false;
#endif
	base::action todo;
	int32_t r, _;
	uint32_t f = 0, t = 0;
	string s, fs, ts, bl = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	if(argc == 1) return printHelp();
	while(--argc > 0)
	{
		s.assign(argv[argc]);
		switch(s[0] == '-' || s[0] == '/' ? ENDIAN(WC2UI16(s[1])) : ENDIAN(WC2UI16(s[0])))
		{
			case 'fr': istringstream(s.substr(3)) >> f; break; // Set from radix
			case 'tr': istringstream(s.substr(3)) >> t; break; // Set to radix
			case 'fs':
			    r = s.find_first_of('=', 3);
			    bl = s.substr(r + 1, s.length() - 1);
			    fs.assign(bl);
			    break; // Set from string
			case 'ts':
			    r = s.find_first_of('=', 3);
			    bl = s.substr(r + 1, s.length() - 1);
			    ts.assign(bl);
			    break; // Set to string
			case 'fl': inUpperCase = false; break; // From lowercase
			case 'tl': outUpperCase = false; break; // To lowercase
			case 'fu': inUpperCase = true; break; // From uppercase
			case 'tu': outUpperCase = true; break; // To uppercase
			case 'fb': case 'f2': fs = bl.substr(0, f =  2); break; // From binary
			case 'tb': case 't2': ts = bl.substr(0, t =  2); break; // To binary
			case 'fo': case 'f8': fs = bl.substr(0, f =  8); break; // From octal
			case 'to': case 't8': ts = bl.substr(0, t =  8); break; // To octal
			case 'fd': case 'f1': fs = bl.substr(0, f = 10); break; // From decimal
			case 'td': case 't1': ts = bl.substr(0, t = 10); break; // To decimal
			case 'fh': case 'fx': fs = bl.substr(0, f = 16); break; // From hexadecimal
			case 'th': case 'tx': ts = bl.substr(0, t = 16); break; // To hexadecimal
			case 'ft': case 'fH': case 'f3': fs = bl.substr(0, f = 36); break; // From hexatrigesimal (base 36)
			case 'tt': case 'Ht': ts = bl.substr(0, t = 36); break; // To hexatrigesimal (base 36)
			case 'fa': case 'fe': fs.resize(256); for(_ = 0, f = 256; _ < 256; _++) fs[_] = (unsigned char)_; break; // Ascii/binary input mode
			case 'ta': case 'te': ts.resize(256); for(_ = 0, asciiout = true, t = 256; _ < 256; _++) ts[_] = (unsigned char)_; break; // Ascii/binary output mode
			case 'ig': case 'sk': case 'dr': todo = base::IGNORE; break; // Ignore invalid characters in input (default)
			case 'ze': case '0\0': todo = base::ZERO;   break; // Zero invalid characters in input
			case 'qu': case 'X\0': case 'x\0': todo = base::QUIT;   break; // Quit read on invalid characters in input
			case 'in': case 'se': case 'al': case 're': todo = base::INFORM; break; // Inform on stderr invalid characters in input
			case 'ex': case 'er': case 'el': todo = base::EXIT;   break; // Exit (with error level) on invalid characters in input
#ifdef DEBUG
			case 'te': case 'rt': tests = true; // Run tests
#endif
			case 'h\0': case '?\0': default: return printHelp(); // Help
		}
	}
	if(f < 2) { cout << "From radix not specified! " << endl; return 1; }
	if(t < 2) { cout << "To radix not specified! Perhaps you specified from radix twice?" << endl; return 1; }
	base b(f, t, fs, ts, inUpperCase, outUpperCase, todo);
	string data(b.readsize(), '\0');
#ifdef DEBUG
	if(tests)
	{
		b.test();
		return 0;
	}
#endif
	for(cin.read(&data[0], b.readsize()); r = cin.gcount(); cin.clear(), data.resize(b.readsize()), cin.read((char *)data.data(), b.readsize()))
	{
		data.resize(r);
		b << data;
		b >> data;
		if(MIN(data.size(), b.writesize()))
		{
			for(_ = b.writesize() - data.size(); _ > 0; _--) cout << b.zero(); // Zero pad
			cout.write(&data[0], MIN(data.size(), b.writesize()));
		}
	}
	cout << flush << flush;
	if(!asciiout) cout << endl;
}
