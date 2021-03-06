/*
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * */
/*
 * stringutils.C
 *
 *  Created on: Apr 9, 2013
 *      Author: xaxaxa
 */
#include <cpoll/cpoll.H>
#include "include/stringutils.H"
#include "include/split.H"
using namespace CP;
namespace cppsp
{
	inline char hexCharToInt(char ch) {
		if (ch <= '9')
			return ch - '0';
		else if (ch <= 'Z')
			return ch - 'A' + 10;
		else return ch - 'a' + 10;
	}
	inline char intToHexChar(char i) {
		if (i < 10)
			return i + '0';
		else return i - 10 + 'A';
	}
	int doURLDecode(const char* in, int inLen, char* out) {
		//XXX: dangerous (potentially exploitable) codepath; please audit
		char* c = out; //points to next byte to be written
		const char* end = in + inLen; //end of input array
		const char* ptr = in; //current read position
		while (ptr < end) {
			const char* next = (const char*) memchr(ptr, '%', end - ptr);
			if (next == NULL) {
				memcpy(c, ptr, end - ptr);
				c += (end - ptr);
				break;
			}
			memcpy(c, ptr, next - ptr); //write out everything between the read position and the '%'
			c += (next - ptr);
			if (next + 2 >= end) { //there isn't 2 bytes after the '%'
				memcpy(c, next, end - next);
				c += (end - next);
				break;
			}
			*c = hexCharToInt(next[1]) << 4 | hexCharToInt(next[2]);
			c++;
			ptr = next + 3;
		}
		return int(c - out);
	}
	int urlDecode(const char* in, int inLen, StreamWriter& sw) {
		//XXX: dangerous (potentially exploitable) codepath; please audit
		char* ch = sw.beginWrite(inLen);
		int len = doURLDecode(in, inLen, ch);
		sw.endWrite(len);
		return len;
	}
	String urlDecode(const char* in, int inLen, StringPool& sp) {
		//XXX: dangerous (potentially exploitable) codepath; please audit
		char* ch = sp.beginAdd(inLen); //output size will never exceed input size
		int len = doURLDecode(in, inLen, ch);
		E: sp.endAdd(len);
		return {ch,len};
	}
	int urlEncode(const char* in, int inLen, CP::StreamWriter& sw) {
		int last_i = 0;
		const char* c = in;
		char ch[3];
		ch[0] = '%';
		int asdf = 0;
		for (int i = 0; i < inLen; i++) {
			if ((48 <= c[i] && c[i] <= 57) || //0-9
					(65 <= c[i] && c[i] <= 90) || //abc...xyz
					(97 <= c[i] && c[i] <= 122) || //ABC...XYZ
					(c[i] == '~' || c[i] == '!' || c[i] == '*' || c[i] == '(' || c[i] == ')'
							|| c[i] == '\'')) continue;
			if (i > last_i) sw.write(in + last_i, i - last_i);
			last_i = i + 1;
			ch[1] = intToHexChar(c[i] >> 4);
			ch[2] = intToHexChar(c[i] & (char) 0xF);
			sw.write(ch, 3);
			asdf += 2;
		}
		if (inLen > last_i) sw.write(in + last_i, inLen - last_i);
		return inLen + asdf;
	}
	std::string urlDecode(const char* in, int inLen) {
		StringStream ss;
		{
			StreamWriter sw(ss);
			urlDecode(in, inLen, sw);
		}
		return ss.str();
	}
	std::string urlEncode(const char* in, int inLen) {
		StringStream ss;
		{
			StreamWriter sw(ss);
			urlEncode(in, inLen, sw);
		}
		return ss.str();
	}
	std::string htmlEscape(const char* in, int inLen) {
		StringStream ss;
		{
			StreamWriter sw(ss);
			htmlEscape(in, inLen, sw);
		}
		return ss.str();
	}
	std::string htmlAttributeEscape(const char* in, int inLen) {
		StringStream ss;
		{
			StreamWriter sw(ss);
			htmlAttributeEscape(in, inLen, sw);
		}
		return ss.str();
	}
	void parseQueryString(const char* in, int inLen, queryStringCallback cb, bool decode) {
		if (decode) {
			MemoryStream ms;
			StreamWriter sw(ms);
			split spl(in, inLen, '&');
			while (spl.read()) {
				const char* s = spl.value.d;
				int l = spl.value.len;
				const char* _end = s + l;
				const char* tmp = (const char*) memchr(s, '=', l);
				if (tmp == NULL) {
					urlDecode(s, l, sw);
					sw.flush();
					cb((const char*) ms.data(), ms.length(), nullptr, 0);
					ms.clear();
				} else {
					urlDecode(s, tmp - s, sw);
					sw.flush();
					int i = ms.length();
					urlDecode(tmp + 1, _end - tmp - 1, sw);
					sw.flush();
					cb((const char*) ms.data(), i, (const char*) (ms.data() + i), ms.length() - i);
					ms.clear();
				}
			}
		} else {
			split spl(in, inLen, '&');
			while (spl.read()) {
				const char* s = spl.value.d;
				int l = spl.value.len;
				const char* _end = s + l;
				const char* tmp = (const char*) memchr(s, '=', l);
				if (tmp == NULL)
					cb(s, l, nullptr, 0);
				else cb(s, tmp - s, tmp + 1, _end - tmp - 1);
			}
		}
	}
	void htmlEscape(const char* in, int inLen, CP::StreamWriter& sw) {
		int sz = 0;
		for (int i = 0; i < inLen; i++) {
			switch (in[i]) {
				case '&':
					sz += 5;
					break;
				case '<':
					sz += 4;
					break;
				case '>':
					sz += 4;
					break;
				default:
					sz++;
					break;
			}
		}

		char* data = sw.beginWrite(sz);
		char* c = data;
		for (int i = 0; i < inLen; i++) {
			switch (in[i]) {
				case '&':
					c[0] = '&';
					c[1] = 'a';
					c[2] = 'm';
					c[3] = 'p';
					c[4] = ';';
					c += 5;
					break;
				case '<':
					c[0] = '&';
					c[1] = 'l';
					c[2] = 't';
					c[3] = ';';
					c += 4;
					break;
				case '>':
					c[0] = '&';
					c[1] = 'g';
					c[2] = 't';
					c[3] = ';';
					c += 4;
					break;
				default:
					*(c++) = in[i];
			}
		}
		sw.endWrite(sz);
	}
	void htmlAttributeEscape(const char* in, int inLen, CP::StreamWriter& sw) {
		int last_i = 0;
		const char* tmp;
		for (int i = 0; i < inLen; i++) {
			switch (in[i]) {
				case '&':
					tmp = "&amp;";
					break;
				case '<':
					tmp = "&lt;";
					break;
				case '>':
					tmp = "&gt;";
					break;
				case '"':
					tmp = "&quot;";
					break;
				case '\'':
					tmp = "&apos;";
					break;
				default:
					continue;
			}
			if (i > last_i) sw.write(in + last_i, i - last_i);
			last_i = i + 1;
			sw.write(tmp);
		}
		if (inLen > last_i) sw.write(in + last_i, inLen - last_i);
	}
	int ci_compare(String s1, String s2) {
		if (s1.length() > s2.length()) return 1;
		if (s1.length() < s2.length()) return -1;
		if (s1.length() == 0) return 0;
		char a, b;
		for (int i = 0; i < s1.length(); i++) {
			a = tolower(s1.data()[i]);
			b = tolower(s2.data()[i]);
			if (a < b) return -1;
			if (a > b) return 1;
		}
		return 0;
	}

	//inline-able memcpy() for copying SHORT STRINGS ONLY
	static inline void memcpy2(void* dst, const void* src, int len) {
		for (int i = 0; i < len; i++)
			((char*) dst)[i] = ((const char*) src)[i];
	}
	// "/"		+ "aaaaa"	=> "/aaaaa"
	// "/asdf/"	+ "zzz"		=> "/asdf/zzz"
	// "/asdf/"	+ "zzz/"		=> "/asdf/zzz/"
	// "/asdf"	+ "zzz"		=> "/zzz"
	// "/asdf/"	+ "../zzz"	=> "/zzz"
	// "/asdf/"	+ "a/../x"	=> "/asdf/x"
	// "/asdf/"	+ "/zzz"		=> "/zzz"
	//the size of buf should be at least strlen(p1)+strlen(p2)
	//returns the length of the string written to buf; does NOT write null byte
	int combinePath(const char* p1, int l1, const char* p2, int l2, char* buf) {
		if (l2 > 0 && p2[0] == '/') {
			memcpy2(buf, p2, l2);
			return l2;
		}
		int i = l1;
		memcpy2(buf, p1, i);
		if (l2 > 0) {
			i--;
			while (i >= 0 && buf[i] != '/')
				i--;
			if (i < 0) i = 0;
			split spl(p2, l2, '/');
			while (spl.read()) {
				const char* s = spl.value.data();
				int l = spl.value.length();
				if (l == 2 && *(const uint16_t*) s == *(const uint16_t*) "..") {
					i--;
					while (i >= 0 && buf[i] != '/')
						i--;
					if (i < 0) i = 0;
				} else if (l == 1 && *s == '.') {
					buf[i] = '/';
					i++;
				} else {
					//while(i>=0 && buf[i]!='/')i--;
					buf[i] = '/';
					i++;
					memcpy2(buf + i, s, l);
					i += l;
				}
			}
			//if (l2 > 0 && i > 0 && p2[l2 - 1] != '/' && buf[i - 1] == '/')
		}
		if (i < 0) i = 0;
		return i;
	}
	int combinePath(const char* p1, const char* p2, char* buf) {
		return combinePath(p1, strlen(p1), p2, strlen(p2), buf);
	}
	//p1 is the "root" directory
	//guarantees that the resulting path won't be outside of p1
	int combinePathChroot(const char* p1, int l1, const char* p2, int l2, char* buf) {
		int i = l1;
		memcpy2(buf, p1, i);
		static const uint16_t parentDir = *(const uint16_t*) "..";
		if (l2 > 0) {
			bool first(true);
			split spl(p2, l2, '/');
			while (spl.read()) {
				const char* s = spl.value.data();
				int l = spl.value.length();
				if (first) {
					first = false;
					if (l == 0) continue;
				}
				if (l == 2 && *(const uint16_t*) s == parentDir) {
					i--;
					while (i >= 0 && buf[i] != '/')
						i--;
					if (i < l1) i = l1;
				} else if (l == 1 && *s == '.') {
					if (!(i > 0 && buf[i - 1] == '/')) {
						buf[i] = '/';
						i++;
					}
				} else {
					//while(i>=0 && buf[i]!='/')i--;
					if (!(i > 0 && buf[i - 1] == '/')) {
						buf[i] = '/';
						i++;
					}
					memcpy2(buf + i, s, l);
					i += l;
				}
			}
			//if (l2 > 0 && i > 0 && p2[l2 - 1] != '/' && buf[i - 1] == '/')
		}
		if (i < l1) i = l1;
		return i;
	}
	int combinePathChroot(const char* p1, const char* p2, char* buf) {
		return combinePathChroot(p1, strlen(p1), p2, strlen(p2), buf);
	}

	String combinePath(String p1, String p2, StringPool& sp) {
		char* tmp = sp.beginAdd(p1.length() + p2.length());
		int l = combinePath(p1.data(), p2.data(), tmp);
		sp.endAdd(l);
		return {tmp,l};
	}
	String combinePathChroot(String p1, String p2, StringPool& sp) {
		char* tmp = sp.beginAdd(p1.length() + p2.length());
		int l = combinePathChroot(p1.data(), p2.data(), tmp);
		sp.endAdd(l);
		return {tmp,l};
	}

	static inline int itoa1(int i, char* b) {
		static char const digit[] = "0123456789";
		char* p = b;
		//negative detection is not needed for this specific use-case
		//(writing the content-length header)
		/*if (i < 0) {
		 *p++ = '-';
		 i = -i;
		 }*/
		p += (i == 0 ? 0 : int(log10f(i))) + 1;
		*p = '\0';
		int l = p - b;
		do { //Move back, inserting digits as u go
			*--p = digit[i % 10];
			i = i / 10;
		} while (i);
		return l;
	}
	//pads beginning with 0s
	//i: input number
	//d: # of digits
	static inline int itoa2(int i, int d, char* b) {
		static char const digit[] = "0123456789";
		for (int x = d - 1; x >= 0; x--) {
			b[x] = digit[i % 10];
			i /= 10;
		}
		return d;
	}
	int rfctime(const tm& time, char* c) {
		static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
		static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
				"Oct", "Nov", "Dec" };
		char* s = c;
		//AAA, AA AAA ???? AA:AA:AA GMT\0
		const char* day = days[time.tm_wday];
		//copy 4 bytes (includes extra null byte)
		*(int*) c = (*(int*) day) | int(',') << 24;
		c += 4;
		*(c++) = ' ';
		c += itoa1(time.tm_mday, c);
		*(c++) = ' ';
		const char* month = months[time.tm_mon];
		*(c++) = *(month++);
		*(c++) = *(month++);
		*(c++) = *(month++);
		*(c++) = ' ';
		c += itoa1(time.tm_year + 1900, c);
		*(c++) = ' ';
		c += itoa2(time.tm_hour, 2, c);
		*(c++) = ':';
		c += itoa2(time.tm_min, 2, c);
		*(c++) = ':';
		c += itoa2(time.tm_sec, 2, c);
		*(c++) = ' ';
		*(c++) = 'G';
		*(c++) = 'M';
		*(c++) = 'T';
		*(c++) = '\0';
		return int(c - s) - 1;
	}
}

