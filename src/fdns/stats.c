#include "fdns.h"
#include "math.h"

typedef struct stats_node_t {
	struct stats_node_t *next;
	char *name;
	float qtime;
	float probability;
} StatsNode;

static StatsNode *stats_list = NULL;
static int stats_cnt = 0;
static double simple_average = 0;

void stats_add(const char *name, float qtime) {
	assert(name);
	assert(qtime);

	// allocate
	stats_cnt++;
	StatsNode *snew = malloc(sizeof(StatsNode));
	if (!snew)
		errExit("malloc");
	memset(snew, 0, sizeof(StatsNode));
	snew->name = strdup(name);
	if (!snew->name)
		errExit("strdup");
	snew->qtime = qtime;

	// find
	StatsNode **pptr = &stats_list;
	while (*pptr != NULL) {
		assert(pptr);
		if ((*pptr)->qtime > qtime)
			break;
		pptr = &((*pptr)->next);
	}

	// add
	snew->next = *pptr;
	*pptr = snew;
	simple_average += snew->qtime;
}


static char *down_list = "";
static int down_cnt = 0;
void stats_down(const char *name) {
	assert(name);

	down_cnt++;
	char *ptr;
	if (asprintf(&ptr, "\t%s\n%s", name, down_list) == -1)
		errExit("asprintf");
	down_list = ptr;
}


void stats_print(void) {
	StatsNode *ptr = stats_list;
	assert(ptr);
	assert(stats_cnt);

	if (down_cnt) {
		printf("\n");
		printf("Servers down:\n%s\n\n", down_list);
	}

	if (stats_cnt <= 1)
		return;
	printf("Simple average %.02f ms\n\n", simple_average / stats_cnt);
	printf("Run-time statistics:\n");

	// calculate probability
	float total = stats_cnt * (stats_cnt - 1);
	int i = 0;
	while (ptr) {
		ptr->probability = ((float) (stats_cnt - 1 - i) * 2 / total);
		i++;
		ptr = ptr->next;
	}

	// adjust probability for random limit
	ptr = stats_list;
	int cnt = 0;
	float sum = 0;
	while (ptr) {
		if (ptr->qtime > QTIME_RANDOM_LIMIT)
			break;
		sum += ptr->probability;
		cnt++;
		ptr = ptr->next;
	}
	if (cnt) {
		sum /= cnt;
		ptr = stats_list;
		for (i = 0; i < cnt; i++, ptr = ptr->next)
			ptr->probability = sum;
	}

	// calculate average;
	float average = 0;
	ptr = stats_list;
	while (ptr) {
		average += ptr->qtime * ptr->probability;
		ptr = ptr->next;
	}

	// calculate standard deviation
	ptr = stats_list;
	float stdev = 0;
	while (ptr) {
		stdev += ptr->probability * powf(ptr->qtime - average, 2);
		ptr = ptr->next;
	}
	stdev = sqrtf(stdev);

	// print
	printf("%-5s%-25s%-12s probability (cumulative)\n", "", "", "query time");
	ptr = stats_list;
	float cumulative = 0;
	i = 1;
	while (ptr) {
		cumulative += ptr->probability;
		char sindex[20];
		sprintf(sindex, "%d", i);
		char sqtime[20];
		sprintf(sqtime, "%.02f ms", ptr->qtime);
		printf("%-5s%-25s%-12s %.02f%% (%.02f%%)\n",
			sindex,
			ptr->name,
			sqtime,
			ptr->probability * 100,
			cumulative * 100);
		ptr = ptr->next;
		i++;
	}
	printf("\n");
	printf("Query time average %.02f ms\n", average);
	printf("Standard deviation %.02f ms\n", stdev);
	printf("\n");
}

