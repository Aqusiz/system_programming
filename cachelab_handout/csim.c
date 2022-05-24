// 2017-16140
#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <getopt.h>
#include <math.h>

int hit = 0, miss = 0, eviction = 0;
int s, E, b, S, B;

typedef struct _line {
	int latest;
	int valid;
	unsigned long long int tag;
	char* block;
} line;

typedef struct _set {
	line* lines;
} set;

typedef struct _cache {
	set* sets;
} cache;

cache init_cache (int setnum, int linenum, int blocknum) {
	cache newcache;
	set newset;
	line newline;
	int setindex, lineindex;

	newcache.sets = (set*) malloc(sizeof(set) * setnum);

	for (setindex = 0; setindex < setnum; setindex++) {
		newset.lines = (line*) malloc(sizeof(line) * linenum);
		newcache.sets[setindex] = newset;

		for (lineindex = 0; lineindex < linenum; lineindex++) {
			newline.latest = 0;
			newline.valid = 0;
			newline.tag = 0;
			newset.lines[lineindex] = newline;
		}
	}
	return newcache;
}

void clean(cache mycache) {
	int i;
	for (i = 0; i < S; i++) {
		set s = mycache.sets[i];
		if (s.lines != NULL) {
			free(s.lines);
		}
	}
	if (mycache.sets != NULL) {
		free(mycache.sets);
	}
}

int emptyline(set s) {
	int i;
	line l;

	for (i = 0; i < E; i++) {
		l = s.lines[i];
		if (l.valid == 0) {
			return i;
		}
	}
	return -1;
}

int evictline(set s, int* usedline) {
	int maxuse = s.lines[0].latest;
	int minuse = s.lines[0].latest;
	int minindex = 0;
	int i;

	for (i = 1; i < E; i++) {
		if (minuse > s.lines[i].latest) {
			minindex = i;
			minuse = s.lines[i].latest;
		}

		if (maxuse < s.lines[i].latest) {
			maxuse = s.lines[i].latest;
		}
	}

	usedline[0] = minuse;
	usedline[1] = maxuse;
	return minindex;
}

void simulate (cache mycache, unsigned long long int addr) {
	int lineindex;
	int full = 1;
	int prevhit = hit;

	int tagsize = (64 - (s + b));
	unsigned long long int inputtag = addr >> (s + b);
	unsigned long long temp = addr << (tagsize);
	unsigned long long setindex = temp >> (tagsize + b);

	set s = mycache.sets[setindex];

	for (lineindex = 0; lineindex < E; lineindex++) {
		if (s.lines[lineindex].valid) {
			if (s.lines[lineindex].tag == inputtag) {
				s.lines[lineindex].latest++;
				hit++;
			}
		}
		else if (!(s.lines[lineindex].valid) && full) {
			full = 0;
		}
	}

	if (prevhit == hit) {
		miss++;
	}
	else {
		return;
	}
	int *usedline = (int*) malloc(sizeof(int) * 2);
	int minindex = evictline(s, usedline);

	if (full) {
		eviction++;
		s.lines[minindex].tag = inputtag;
		s.lines[minindex].latest = usedline[1] + 1;
	}

	else {
		int emptyindex = emptyline(s);
		s.lines[emptyindex].tag = inputtag;
		s.lines[emptyindex].valid = 1;
		s.lines[emptyindex].latest = usedline[1] + 1;
	}
	free(usedline);
}

int main (int argc, char* argv[]) {
	int opt;
	char inst;
	char* tracefilename;
	cache mycache;
	unsigned long long int addr;
	int size;

	while ( (opt = getopt(argc, argv, "s:E:b:t:h")) != -1) {
		switch(opt) {
			case 's': s = atoi(optarg);
					  break;
			case 'E': E = atoi(optarg);
					  break;
			case 'b': b = atoi(optarg);
					  break;
			case 't': tracefilename = optarg;
					  break;
			case 'h': exit(0);
			default: exit(1);
		}
	}

	S = pow(2.0, s);
	B = pow(2.0, b);
	mycache = init_cache (S, E, B);
	FILE* tracefile = fopen(tracefilename, "r");

	if (tracefile != NULL) {
		while (fscanf(tracefile, "%c %llx,%d", &inst, &addr, &size) != EOF) {
			switch(inst) {
				case 'I': break;
				case 'L': simulate(mycache, addr);
						  break;
				case 'S': simulate(mycache, addr);
						  break;
				case 'M': simulate(mycache, addr);
						  simulate(mycache, addr);
						  break;
				default: break;
			}
		}
	}

	printSummary(hit, miss, eviction);
	clean(mycache);
	fclose(tracefile);
	return 0;
}
