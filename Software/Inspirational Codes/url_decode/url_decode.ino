#include <string.h>

const char *tests[] = {
  "", // no parameters
  "param1=test", // simple test
  "param1=test&param2=test2", // two parameters
  "param1=test&param2=test+2", // parameter with an encoded space
  "param1=test&param2=c%3A%5Cfoodir%5Cbarfile.fil", // percent encoding
  "p1=1&p2=2&p3=3&p4=4&p5=5&p6=6&p7=7&p8=8" // more params than our test will acommodate
};

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

}

void loop() {
  char buf[100];
  char *params[5][2];

  delay(5000);

  for (int i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
    Serial.print("parsing \"");
    Serial.print(tests[i]);

    // copy test[i] into the buffer
    // because the parser overwrites what is i the string it is passed.
    strcpy(buf, tests[i]);

    // parse the buffer into params[][]
    int resultsCt = parseUrlParams(buf, params, 5, true);

    // print off the results;

    Serial.print("\" produced ");
    Serial.print(resultsCt);
    Serial.print(" parameters:");
    Serial.println();

    for (int i = 0; i < resultsCt; i++) {
      Serial.print("param ");
      Serial.print(i);
      Serial.print(" name \"");
      Serial.print( params[i][0]);
      Serial.print("\", param \"");
      Serial.print( params[i][1]);
      Serial.print("\".");
      Serial.println();
    }
    Serial.println();
  }
}

/**
 * queryString: the string which is to be parsed.
 * WARNING! This function overwrites the content of this string. Pass this function a copy
 * if you need the value preserved.
 * results: place to put the pairs of param name/value.
 * resultsMaxCt: maximum number of results, = sizeof(results)/sizeof(*results)
 * decodeUrl: if this is true, then url escapes will be decoded as per RFC 2616
 */

int parseUrlParams(char *queryString, char *results[][2], int resultsMaxCt, boolean decodeUrl) {
  int ct = 0;

  while (queryString && *queryString && ct < resultsMaxCt) {
    results[ct][0] = strsep(&queryString, "&");
    results[ct][1] = strchrnul(results[ct][0], '=');
    if (*results[ct][1]) *results[ct][1]++ = '\0';

    if (decodeUrl) {
      percentDecode(results[ct][0]);
      percentDecode(results[ct][1]);
    }

    ct++;
  }

  return ct;
}

/**
 * Perform URL percent decoding.
 * Decoding is done in-place and will modify the parameter.
 */

void percentDecode(char *src) {
  char *dst = src;

  while (*src) {
    if (*src == '+') {
      src++;
      *dst++ = ' ';
    }
    else if (*src == '%') {
      // handle percent escape

      *dst = '\0';
      src++;

      if (*src >= '0' && *src <= '9') {
        *dst = *src++ - '0';
      }
      else if (*src >= 'A' && *src <= 'F') {
        *dst = 10 + *src++ - 'A';
      }
      else if (*src >= 'a' && *src <= 'f') {
        *dst = 10 + *src++ - 'a';
      }

      // this will cause %4 to be decoded to ascii @, but %4 is invalid
      // and we can't be expected to decode it properly anyway

      *dst <<= 4;

      if (*src >= '0' && *src <= '9') {
        *dst |= *src++ - '0';
      }
      else if (*src >= 'A' && *src <= 'F') {
        *dst |= 10 + *src++ - 'A';
      }
      else if (*src >= 'a' && *src <= 'f') {
        *dst |= 10 + *src++ - 'a';
      }

      dst++;
    }
    else {
      *dst++ = *src++;
    }

  }
  *dst = '\0';
}
