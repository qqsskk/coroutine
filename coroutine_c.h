#ifndef _COROUTINE_H_
#define _COROUTINE_H_

/*
*C���Կ�ƽ̨�˳̿�
*@author:yuqingtong
*@email:yuqingtong1990@gmail.com
*@date:2018.12.13
*/

/**
�˳̴���״̬
*/
typedef enum costatus{       
	CS_Dead = 0,        // �˳�����״̬
	CS_Ready = 1,       // �˳��Ѿ�����
	CS_Running = 2,     // �˳���������
	CS_Suspend = 3,     // �˳���ͣ�ȴ�
}costatus_e;

typedef struct comng* comng_t;

/*
* ���������˳̵�����, ��ͬ���˳̴�����Ҫִ�еĺ�����.
* schedule : co_start �������صĽ��
* ud       : �û��Զ�������
*/
typedef void(*co_fuc)(comng_t comng, void * ud);

/*
* �����˳�ϵͳ, ���������˳�
*            : ���ؿ������˳̵���ϵͳ������
*/
extern comng_t co_start(void);

/*
* �رտ������˳�ϵͳ
* comng    : co_start ���ص��˳̹�����
*/
extern void co_close(comng_t comng);

/*
* ����һ���˳̶���,�����ش����˳̵�id. �����õ��˳�״̬��CS_Ready
* comng    : co_start ���ص��˳̹�����
* func     : �˳����е�����
* ud       : �û����������, co_f �� ud ��ʹ��
*          : ���ش����õ��˳̱�ʶid
*/
extern int co_create(comng_t comng, co_fuc func, void * ud);

/*
* ��������˳̶���.
* comng    : �˳̹���������
* id       : co_create �������˳̶���
*/
extern void co_resume(comng_t comng, int id);

/*
* �жϵ�ǰ���еĵ��˳�, ����CPU�������˳̴������.
* comng    : �˳̹���������
*/
extern void co_yield(comng_t comng);

/*
* �õ���ǰ�˳����е�״̬
* comng    : �˳̹���������
* id       : co_create �������˳̶���
*          : ����״̬������� costatus_e
*/
extern costatus_e co_status(comng_t comng, int id);

/*
* �õ���ǰ�˳�ϵͳ�����е��˳�, ���� < 0��ʾû���˳�������
* comng    : �˳̹���������
*          : ���ص�ǰ���е��˳̱�ʶid,
*/
extern int co_running(comng_t comng);

#endif
