#include "coroutine.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
	#include <Windows.h>
#else
	#if __APPLE__ && __MACH__
		#include <sys/ucontext.h>
	#else 
		#include <ucontext.h>
		#include <stddef.h>
		#include <stdint.h>
	#endif 
#endif

// �˳�ջ��С
#define _INT_STACK        (1024 * 1024)
#define _INT_COROUTINE    (16)

struct coroutine;

struct comng {
	int running;                // ��ǰ�˳̹����������е��˳�id
	int nco;                    // ��ǰ�˳̼���ѯ�е�ǰ����
	int cap;                    // �˳̼�����,
	struct coroutine ** co;     // ������˳̼�
#ifdef WIN32
	PVOID main;                 // �˳̹������б������ʱ�˳̶���
#else
	char stack[_INT_STACK];
	ucontext_t main;            // �˳̹������б������ʱ�˳̶���
#endif
};

struct coroutine {
#ifdef WIN32
	PVOID ctx;                    // ����ϵͳ�˳̶���               
#else
	char * stack;
	ucontext_t ctx;               // ����ϵͳ�˳̶���
	ptrdiff_t cap;
	ptrdiff_t size;
#endif

	coroutine_fuc func;                    // �˳�ִ�еĺ�����
	void * ud;                    // �˳�ִ�еĶ������
	costatus_e status;            // ��ǰ�˳�����״̬
	struct comng * comng;         // ��ǰ�˳̼�������
};

static inline struct coroutine * _co_new(comng_t comng, coroutine_fuc func, void * ud) {
	struct coroutine * co = malloc(sizeof(struct coroutine));
	assert(co && comng && func);
	co->func = func;
	co->ud = ud;
	co->comng = comng;
	co->status = CS_Ready;
#ifndef WIN32
	co->cap = 0;
	co->size = 0;
	co->stack = NULL;
#endif
	return co;
}

static inline void _co_delete(struct coroutine * co) {
	#ifdef WIN32
		DeleteFiber(co->ctx);
	#else
		free(co->stack);
	#endif
	free(co);
}

#ifdef WIN32
static inline VOID WINAPI _comain(LPVOID ptr) {
	struct comng * comng = ptr;
	int id = comng->running;
	struct coroutine * co = comng->co[id];
	co->func(comng, co->ud);
	_co_delete(co);
	comng->co[id] = NULL;
	--comng->nco;
	comng->running = -1;
}
#else
static inline void _comain(uint32_t low32, uint32_t hig32) {
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hig32 << 32);
	struct comng * comng = (struct comng *)ptr;
	int id = comng->running;
	struct coroutine * co = comng->co[id];
	co->func(comng, co->ud);
	_co_delete(co);
	comng->co[id] = NULL;
	--comng->nco;
	comng->running = -1;
}

static void _save_stack(struct coroutine * co, char * top) {
	char dummy = 0;
	assert(top - &dummy <= _INT_STACK);
	if (co->cap < top - &dummy) {
		free(co->stack);
		co->cap = top - &dummy;
		co->stack = malloc(co->cap);
		assert(co->stack);
	}
	co->size = top - &dummy;
	memcpy(co->stack, &dummy, co->size);
}
#endif // WIN32

extern comng_t coroutine_start(void)
{
	struct comng * comng = malloc(sizeof(struct comng));
	assert(NULL != comng);
	comng->nco = 0;
	comng->running = -1;
	comng->co = calloc(comng->cap = _INT_COROUTINE, sizeof(struct coroutine *));
	assert(NULL != comng->co);
	
#ifdef WIN32
	comng->main = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);// ����WindowЭ��
#endif	
	return comng;	
}

extern void coroutine_close(comng_t comng)
{
	int i;
	for (i = 0; i < comng->cap; ++i) {
		struct coroutine * co = comng->co[i];
		if (co) {
			_co_delete(co);
			comng->co[i] = NULL;
		}
	}
	free(comng->co);
	comng->co = NULL;
	free(comng);
}

extern int coroutine_create(comng_t comng, coroutine_fuc func, void * ud)
{
	struct coroutine * co = _co_new(comng, func, ud);
	// ��������ͨ���, �����Ҽ�
	if (comng->nco < comng->cap) {
		int i;
		for (i = 0; i < comng->cap; ++i) {
			int id = (i + comng->nco) % comng->cap;
			if (NULL == comng->co[id]) {
				comng->co[id] = co;
				++comng->nco;
				return id;
			}
		}
		assert(i == comng->cap);
		return -1;
	}

	// ��Ҫ���·���ռ�, ������Ϻ󷵻�
	comng->co = realloc(comng->co, sizeof(struct coroutine *) * comng->cap * 2);
	assert(NULL != comng->co);
	memset(comng->co + comng->cap, 0, sizeof(struct coroutine *) * comng->cap);
	comng->cap <<= 1;
	comng->co[comng->nco] = co;
	return comng->nco++;
}

void coroutine_yield(comng_t comng)
{
	struct coroutine * co;
	int id = comng->running;
	assert(id >= 0);
	co = comng->co[id];
	#ifdef WIN32
		co->status = CS_Suspend;
		comng->running = -1;
		co->ctx = GetCurrentFiber();
		SwitchToFiber(comng->main);
	#else
		assert((char *)&co > comng->stack);
		_save_stack(co, comng->stack + _INT_STACK);
		co->status = CS_Suspend;
		comng->running = -1;
		swapcontext(&co->ctx, &comng->main);
	#endif
}

extern void coroutine_resume(comng_t comng, int id)
{
	struct coroutine * co;
#ifndef WIN32
	uintptr_t ptr;
#endif
	assert(comng->running == -1 && id >= 0 && id < comng->cap);
	co = comng->co[id];

	if (NULL == co || co->status == CS_Dead)
		return;

	if (CS_Ready == co->status)
	{
		comng->running = id;
		co->status = CS_Running;
#ifdef WIN32
		co->ctx = CreateFiberEx(_INT_STACK, 0, FIBER_FLAG_FLOAT_SWITCH, _comain, comng);
		comng->main = GetCurrentFiber();
		SwitchToFiber(co->ctx);
#else
		getcontext(&co->ctx);
		co->ctx.uc_stack.ss_sp = comng->stack;
		co->ctx.uc_stack.ss_size = _INT_STACK;
		co->ctx.uc_link = &comng->main;
		ptr = (uintptr_t)comng;
		makecontext(&co->ctx, (void(*)())_comain, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));
		swapcontext(&comng->main, &co->ctx);
#endif
	}
	else if (CS_Suspend == co->status)
	{
		comng->running = id;
		co->status = CS_Running;
#ifdef WIN32
		comng->main = GetCurrentFiber();
		SwitchToFiber(co->ctx);
#else
		memcpy(comng->stack + _INT_STACK - co->size, co->stack, co->size);
		swapcontext(&comng->main, &co->ctx);
#endif
	}
	else
	{
		assert(0);
	}

}

extern costatus_e coroutine_status(comng_t comng, int id)
{
	assert(comng && id >= 0 && id < comng->cap);
	return comng->co[id] ? comng->co[id]->status : CS_Dead;
}

extern int coroutine_running(comng_t comng)
{
	return comng->running;
}
