#define _GNU_SOURCE
#include <dlfcn.h> 

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
//build: gcc -o deadlock deadlock.c -lpthread -ldl


#if 1       //graph

typedef unsigned long int uint64;


#define MAX		100

enum Type {PROCESS, RESOURCE};

struct source_type {

	uint64 id;
	enum Type type;

	uint64 lock_id;
	int degress;
};

struct vertex {

	struct source_type s;
	struct vertex *next;

};

struct task_graph {

	struct vertex list[MAX];
	int num;

	struct source_type locklist[MAX];
	int lockidx; //

	pthread_mutex_t mutex;
};

struct task_graph *tg = NULL;
int path[MAX+1];
int visited[MAX];
int k = 0;
int deadlock = 0;
//创建一个新的顶点，并初始化其数据和指针。
struct vertex *create_vertex(struct source_type type) {

	struct vertex *tex = (struct vertex *)malloc(sizeof(struct vertex ));

	tex->s = type;
	tex->next = NULL;

	return tex;

}

//在图中查找指定类型的节点，返回其索引；如果未找到，返回 -1。
int search_vertex(struct source_type type) {

	int i = 0;

	for (i = 0;i < tg->num;i ++) {

		if (tg->list[i].s.type == type.type && tg->list[i].s.id == type.id) {
			return i;
		}

	}

	return -1;
}

//如果节点不存在，则将其添加到图中。
void add_vertex(struct source_type type) {

	if (search_vertex(type) == -1) {

		tg->list[tg->num].s = type;
		tg->list[tg->num].next = NULL;
		tg->num ++;

	}

}

//在图中添加一条边，从 from 节点指向 to 节点。
int add_edge(struct source_type from, struct source_type to) {

	add_vertex(from);
	add_vertex(to);

	struct vertex *v = &(tg->list[search_vertex(from)]);

	while (v->next != NULL) {
		v = v->next;
	}

	v->next = create_vertex(to);

}

//检查是否存在从 i 到 j 的边。
int verify_edge(struct source_type i, struct source_type j) {

	if (tg->num == 0) return 0;

	int idx = search_vertex(i);
	if (idx == -1) {
		return 0;
	}

	struct vertex *v = &(tg->list[idx]);

	while (v != NULL) {

		if (v->s.id == j.id) return 1;

		v = v->next;
		
	}

	return 0;

}

//删除从 from 到 to 的边。
int remove_edge(struct source_type from, struct source_type to) {

	int idxi = search_vertex(from);
	int idxj = search_vertex(to);

	if (idxi != -1 && idxj != -1) {

		struct vertex *v = &tg->list[idxi];
		struct vertex *remove;

		while (v->next != NULL) {

			if (v->next->s.id == to.id) {

				remove = v->next;
				v->next = v->next->next;

				free(remove);
				break;

			}

			v = v->next;
		}

	}

}

//打印检测到的死锁路径。
void print_deadlock(void) {

	int i = 0;

	printf("cycle : ");
	for (i = 0;i < k-1;i ++) {

		printf("%ld --> ", tg->list[path[i]].s.id);

	}

	printf("%ld\n", tg->list[path[i]].s.id);

}
//深度优先搜索（DFS）遍历图，检测是否存在环。如果访问到已访问过的节点，则说明存在环，打印死锁路径。
int DFS(int idx) {

	struct vertex *ver = &tg->list[idx];
	if (visited[idx] == 1) {

		path[k++] = idx;
		print_deadlock();
		deadlock = 1;
		
		return 0;
	}

	visited[idx] = 1;
	path[k++] = idx;

	while (ver->next != NULL) {

		DFS(search_vertex(ver->next->s));
		k --;
		
		ver = ver->next;

	}

	
	return 1;

}

//从指定节点开始，检查图中是否存在环
int search_for_cycle(int idx) {

	struct vertex *ver = &tg->list[idx];
	visited[idx] = 1;
	k = 0;
	path[k++] = idx;

	while (ver->next != NULL) {

		int i = 0;
		for (i = 0;i < tg->num;i ++) {
			if (i == idx) continue;
			
			visited[i] = 0;
		}

		for (i = 1;i <= MAX;i ++) {
			path[i] = -1;
		}
		k = 1;

		DFS(search_vertex(ver->next->s));
		ver = ver->next;
	}

}


#endif

#if 1

int search_lock(uint64 lock)
{
	int i = 0;
	for(i = 0; i < tg->lockidx; i++)
	{
		if (tg->locklist[i].lock_id == lock)
		{
			return i;
		}
		
	}
	return -1;
}

int search_empty_lock(uint64 lock)
{
	int i = 0;
	for(i = 0; i < tg->lockidx; i ++)
	{
		if(tg->locklist[i].lock_id == 0)
		{
			return i;
		}
	}

	return tg->lockidx;
}

void before_lock(uint64_t tid,uint64_t lockaddr)
{
	int idx;
	for (idx = 0; idx < tg->lockidx; idx ++)
	{
		if(tg->locklist[idx].lock_id == lockaddr)
		{
			struct source_type from;
			from.id = tid;
			from.type = PROCESS;
			add_vertex(from);

			struct source_type to;
			to.id = tg->locklist[idx].id;
			to.type = PROCESS;
			add_vertex(to);

			tg->locklist[idx].degress ++;

			if(!verify_edge(from, to))
			{
				add_edge(from, to);
			}
		}
	}
}

void after_lock(uint64_t tid,uint64_t lockaddr)
{
	int idx = 0;
	//资源不在表里
	if (-1 == (idx = search_lock(lockaddr)))
	{
		int eidx = search_empty_lock(lockaddr);
		tg->locklist[eidx].id = tid;
		tg->locklist[eidx].lock_id = lockaddr;

		tg->lockidx ++;
	}
	//资源在表里
	else
	{
			struct source_type from;
			from.id = tid;
			from.type = PROCESS;
			add_vertex(from);

			struct source_type to;
			to.id = tg->locklist[idx].id;
			to.type = PROCESS;
			add_vertex(to);

			tg->locklist[idx].degress --;
			if(verify_edge(from, to))
			{
				remove_edge(from, to);
			}
			tg->locklist[idx].id = tid;
	}
}

void after_unlock(uint64_t tid,uint64_t lockaddr)
{
	int idx = search_lock(lockaddr);

	if (tg->locklist[idx].degress == 0)	
	{
		tg->locklist[idx].id = 0;
		tg->locklist[idx].lock_id = 0;
	}
	
}

void check_dead_lock(void)
{
	int i = 0;
	deadlock = 0;
	for ( i = 0; i < tg->num; i++)
	{
		if (deadlock == 1)
		{
			break;
		}
		search_for_cycle(i);
		
	}
	if (deadlock == 0)
	{
		printf("no deadlock\n");
	}
	
	
}

static void *thread_routine(void *args)
{
	while (1)
	{		
		sleep(3);
		check_dead_lock();
	}
	
}

void start_check(void)
{
	tg = (struct task_graph*)malloc(sizeof(struct task_graph));
	tg->num = 0;
	tg->lockidx = 0;
	
	pthread_t tid;

	pthread_create(&tid, NULL, thread_routine, NULL);
}

#endif




#if 1
//定义原型
typedef int (*pthread_mutex_lock_t)(pthread_mutex_t *mtx);
pthread_mutex_lock_t pthread_mutex_lock_f = NULL;

typedef int (*pthread_mutex_unlock_t)(pthread_mutex_t *mtx);
pthread_mutex_unlock_t pthread_mutex_unlock_f = NULL;

int pthread_mutex_lock(pthread_mutex_t *mtx)
{
	pthread_t selfid = pthread_self();
    before_lock(selfid, (uint64_t)mtx);

    pthread_mutex_lock_f(mtx);
    
	after_lock(selfid, (uint64_t)mtx);

}

int pthread_mutex_unlock(pthread_mutex_t *mtx)
{
    pthread_mutex_unlock_f(mtx);

    pthread_t selfid = pthread_self();
    after_unlock(selfid, (uint64_t)mtx);
}

void init_hook(void)
{
    if (!pthread_mutex_lock_f)
    {
        pthread_mutex_lock_f = dlsym(RTLD_NEXT, "pthread_mutex_lock");
    }
    if (!pthread_mutex_unlock_f)
    {
        pthread_mutex_unlock_f = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
    }
    
}
#endif

#if 1  //debug
pthread_mutex_t mtx1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx4 = PTHREAD_MUTEX_INITIALIZER;
void *t1_cb(void *arg)
{
	printf("t1: %ld\n", pthread_self());
    pthread_mutex_lock(&mtx1);
    sleep(1);

    pthread_mutex_lock(&mtx2);
    
    pthread_mutex_unlock(&mtx2);

    pthread_mutex_unlock(&mtx1);
}

void *t2_cb(void *arg)
{
	printf("t2: %ld\n", pthread_self());
    pthread_mutex_lock(&mtx2);
    sleep(1);

    pthread_mutex_lock(&mtx3);
    
    pthread_mutex_unlock(&mtx3);

    pthread_mutex_unlock(&mtx2);
}

void *t3_cb(void *arg)
{
	printf("t3: %ld\n", pthread_self());
    pthread_mutex_lock(&mtx3);
    sleep(1);

    pthread_mutex_lock(&mtx4);
    
    pthread_mutex_unlock(&mtx4);

    pthread_mutex_unlock(&mtx3);

}

void *t4_cb(void *arg)
{
	printf("t4: %ld\n", pthread_self());
    pthread_mutex_lock(&mtx4);
    sleep(1);

    pthread_mutex_lock(&mtx1);
    
    pthread_mutex_unlock(&mtx1);

    pthread_mutex_unlock(&mtx4);
}



int main(int argc, char const *argv[])
{

    init_hook();
	start_check();
    pthread_t t1, t2, t3, t4;
    pthread_create(&t1, NULL, t1_cb, NULL);
    pthread_create(&t2, NULL, t2_cb, NULL);
    pthread_create(&t3, NULL, t3_cb, NULL);
    pthread_create(&t4, NULL, t4_cb, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);
    printf("finish\n");
    return 0;
}
#endif

#if 1

#endif