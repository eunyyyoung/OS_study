//avoiding deadlock_20143075
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>

#define SEMPERM 0600
#define TRUE 1
#define FALSE 0
#define MAX 2  //maximum 왼손,오른손
int AllocB = 0;  //C가 잡은 젓가락 갯수

typedef union   _semun {
             int val;
             struct semid_ds *buf;
             ushort *array;
             } semun;

int initsem (key_t semkey, int n) {
   int status = 0, semid;
   if ((semid = semget (semkey, 1, SEMPERM | IPC_CREAT | IPC_EXCL)) == -1)
   {
       if (errno == EEXIST)
                semid = semget (semkey, 1, 0);
   }
   else
   {
       semun arg;
       arg.val = n;
       status = semctl(semid, 0, SETVAL, arg);
   }
   if (semid == -1 || status == -1)
   {
       perror("initsem failed");
       return (-1);
   }
   return (semid);
}

int p (int semid) {
   struct sembuf p_buf;
   p_buf.sem_num = 0;
   p_buf.sem_op = -1;
   p_buf.sem_flg = SEM_UNDO;
   if (semop(semid, &p_buf, 1) == -1)
   {
      printf("p(semid) failed");
      exit(1);
   }
   return (0);
}

int v (int semid) {
   struct sembuf v_buf;
   v_buf.sem_num = 0;
   v_buf.sem_op = 1;
   v_buf.sem_flg = SEM_UNDO;
   if (semop(semid, &v_buf, 1) == -1)
   {
      printf("v(semid) failed");
      exit(1);
   }
   return (0);
}

// Class Lock
typedef struct _lock {
   int semid;  // 세마포 1개로 열림/잠김 및 waiting queue의 상태까지 모두 표현된다.
} Lock;

void initLock(Lock *l, key_t semkey) {
   if ((l->semid = initsem(semkey,1)) < 0)
   // 세마포를 연결한다.(없으면 초기값을 1로 주면서 새로 만들어서 연결한다.)
      exit(1);
}

void Acquire(Lock *l) {
   p(l->semid);
}

void Release(Lock *l) {
   v(l->semid);
}

// Shared variable by file
void reset(char *fileVar,int temp, Lock *l) {
// fileVar라는 이름의 텍스트 화일을 없으면 새로 만들고 temp값을 기록한다.(temp로 초기화)
  Acquire(l);
  if(access(fileVar, F_OK) == -1)
  {
    pid_t pid;   //프로세스 id 받아옴
    pid = getpid();

    time_t now;   // 현재시간 받아옴
    now = time(NULL);
    struct tm* d;
    d = localtime(&now);

    FILE *fp;
    fp = fopen(fileVar, "w");  //a
    fprintf(fp, "pid: %d,  time: %02d:%02d:%02d,  %d\n", pid, d->tm_hour, d->tm_min, d->tm_sec , temp);
    fclose(fp);
  }
  Release(l);
}

void Store(char *fileVar,int i) {
// fileVar 화일 끝에 i 값을 append한다.
  pid_t pid;   //프로세서 id 받아옴
  pid = getpid();

  time_t now;   // 현재시간 받아옴
  now = time(NULL);
  struct tm* d;
  d = localtime(&now);

  FILE *fp;
  fp = fopen(fileVar, "a");
  fprintf(fp, "pid: %d,  time: %02d:%02d:%02d,  %d\n",pid, d->tm_hour, d->tm_min, d->tm_sec , i);
  fclose(fp);
}

int Load(char *fileVar) {
// fileVar 화일의 마지막 값을 읽어 온다.
  FILE *fp;
  int lastdata;

  while (1) {    // 만약 빈 파일이면 파일 읽기 다시 시도
      fp = fopen(fileVar, "r");
      if (fgetc(fp) != EOF) {break;}
      fclose(fp);
  }

  while (fgetc(fp) != EOF) {   //read the last data
    fscanf(fp, "%d", &lastdata);
  }
  fclose(fp);
  return lastdata;
}

void add(char *fileVar,int i) {
// fileVar 화일의 마지막 값을 읽어서 i를 더한 후에 이를 끝에 append 한다.
  pid_t pid;   //프로세서 id 받아옴
  pid = getpid();

  time_t now;   // 현재시간 받아옴
  now = time(NULL);
  struct tm* d;
  d = localtime(&now);

  int lastdata = Load(fileVar);
  FILE *fp;

  fp = fopen(fileVar, "a");  //a+??
  fprintf(fp, "pid: %d,  time: %02d:%02d:%02d,  %d\n", pid, d->tm_hour, d->tm_min, d->tm_sec, lastdata + i);
  fclose(fp);
}

void sub(char *fileVar, int i) {
// fileVar 화일의 마지막 값을 읽어서 i를 뺀 후에 이를 끝에 append 한다.
  pid_t pid;   //프로세서 id 받아옴
  pid = getpid();

  time_t now;   // 현재시간 받아옴
  now = time(NULL);
  struct tm* d;
  d = localtime(&now);

  int lastdata = Load(fileVar);
  FILE *fp;

  fp = fopen(fileVar, "a");   //a+??
  fprintf(fp, "pid: %d,  time: %02d:%02d:%02d,  %d\n", pid, d->tm_hour, d->tm_min, d->tm_sec, lastdata - i);
  fclose(fp);
}


// Class CondVar
typedef struct _cond {
   int semid;
   char *queueLength;
} CondVar;

void initCondVar(CondVar *c, key_t semkey, char *queueLength,  Lock *l) {
   c->queueLength = queueLength;
   reset(c->queueLength,0,l); // queueLength 0으로 초기화
   if ((c->semid = initsem(semkey,0)) < 0)
   // 세마포를 연결한다.(없으면 초기값을 0로 주면서 새로 만들어서 연결한다.)
      exit(1);
}

void Wait(CondVar *c, Lock *lock) {
  add(c->queueLength, 1) ;
  Release(lock);
  p(c->semid);
  Acquire(lock);
}

void Signal(CondVar *c) {
  if(Load(c->queueLength)>0){   //기다리는 사람 있으면 깨운다.
    v(c->semid);
    sub(c->queueLength, 1);
  }
}

void Broadcast(CondVar *c) {
  while(Load(c->queueLength)>0){  //기다리는 사람 있으면 모두 깨운다.
    v(c->semid);
    sub(c->queueLength, 1);
  }
}


void Take_R2(char *R2, CondVar *C2, Lock *L1,char* AvailR,char* isSafe){
  Acquire(L1);
  pid_t pid;   //프로세서 id 받아옴
  pid = getpid();
  //R2가 0이면 젓가락이 없어서 기다린다 or
  //requestB가 AvailR보다 크고 & 젓가락 2개 갖고 있는 사람이 없으면 기다린다.
  while(Load(R2)==0 || ((MAX-AllocB)>Load(AvailR) && Load(isSafe)==0)){
    if(Load(isSafe)==0)
      printf("**deadlock warning**Each one has one R except (B)\n");
    printf("process %d (B) wait R2(take)\n", pid);
    Wait(C2, L1);
    printf("process %d (B) wakes up waiting for R2\n", pid);
  }
  Store(R2,0);
  printf("process %d (B) gets R2\n", pid);
  sub(AvailR, 1);
  AllocB++;
  Release(L1);
}

void Take_R3(char *R3, CondVar *C3, Lock *L1,char* AvailR,char* isSafe){
  Acquire(L1);
  pid_t pid;   //프로세서 id 받아옴
  pid = getpid();
//R3가 0이면 젓가락이 없어서 기다린다 or requestB가 AvailR보다 크면 기다린다
  while(Load(R3)==0 || (MAX-AllocB)>Load(AvailR)){
    printf("process %d (B) wait R3(take)\n", pid);
    Wait(C3, L1);
    printf("process %d (B) wakes up waiting for R3\n", pid);
  }
  Store(R3,0);
  printf("process %d (B) gets R3\n", pid);
  sub(AvailR, 1);
  AllocB++;
  add(isSafe,1); //젓가락 2개 갖는 사람이 있다.(밥 먹는 사람)
  Release(L1);
}

void Put_R2(char *R2, CondVar *C2, Lock *L1,char* AvailR,char* isSafe){
  Acquire(L1);
  pid_t pid;   //프로세서 id 받아옴
  pid = getpid();

  Store(R2,1);
  printf("process %d (B) puts R2 down\n", pid);
  add(AvailR,1);
  AllocB--;
  sub(isSafe,1);  //젓가락 2개 갖는 사람 없다.
  Signal(C2); //젓가락 R2 기다리는 사람 있으면 깨운다.
  printf("process %d (B) wait R2(put)\n", pid);
  Release(L1);
}

void Put_R3(char *R3, CondVar *C3, Lock *L1,char* AvailR){
  Acquire(L1);
  pid_t pid;   //프로세서 id 받아옴
  pid = getpid();

  Store(R3,1);
  printf("process %d (B) puts R3 down\n", pid);
  add(AvailR,1);
  AllocB--;
  Signal(C3); //젓가락 R3 기다리는 사람 있으면 깨운다.
  printf("process %d (B) wait R3(put)\n", pid);
  Release(L1);
}

void Phil_B(char *R2, char *R3, CondVar *C2, CondVar *C3, Lock *L1, char* AvailR,char* isSafe){
  pid_t pid;   //프로세서 id 받아옴
  pid = getpid();

  Take_R2(R2, C2, L1, AvailR, isSafe);  //젓가락 R2 집는다.
  printf("process %d (B) starts thinking\n", pid);
  sleep(1);  //thinking하는 시간
  printf("process %d (B) stops thinking\n", pid);
  Take_R3(R3, C3, L1, AvailR,isSafe);  //젓가락 R3 집는다.
  printf("process %d (B) starts eating\n", pid);
  sleep(2);  //eating하는 시간
  printf("process %d (B) stops eating\n", pid);
  Put_R2(R2, C2, L1, AvailR, isSafe);    //젓가락 R2 내려놓는다.
  Put_R3(R3, C3, L1, AvailR);            //젓가락 R3 내려놓는다.
  //sleep(0.2);
}

void main() {
  pid_t pid;   //프로세서 id 받아옴
  pid = getpid();

  time_t now;   // 현재시간 받아옴
  now = time(NULL);
  struct tm* d;
  d = localtime(&now);
//시작할때 pid와 시간 표시
  printf("\n-------- Phil_B Start - pid: %d,  time: %02d:%02d:%02d\n", pid, d->tm_hour, d->tm_min, d->tm_sec);

  key_t semkey_lock1 = 0x201;
  key_t semkey_c1 = 0x301;
  key_t semkey_c2 = 0x302;
  key_t semkey_c3 = 0x303;

  Lock L1;
  CondVar C1;
  CondVar C2;
  CondVar C3;

  initLock(&L1, semkey_lock1);
  initCondVar(&C1, semkey_c1 , "C1.txt" ,&L1);
  initCondVar(&C2, semkey_c2 , "C2.txt", &L1);
  initCondVar(&C3, semkey_c3 , "C3.txt", &L1);
  //화일변수
  char* R1 = "R1.txt";
  char* R2 = "R2.txt";
  char* R3 = "R3.txt";
  char* AvailR = "AvailR.txt";
  char* isSafe = "isSafe.txt";

  // 화일변수 생성 & 초기화
  reset(R1,1, &L1); reset(R2,1, &L1); reset(R3,1, &L1); //젓가락 R1,R2,R3 1로 초기화
  reset(AvailR, 3, &L1);  //가용 젓가락 3으로 초기화 (젓가락 3개)
  reset(isSafe, 0, &L1);  //젓가락 2개라 밥을 먹는 사람 유무 (없으면 0,있으면 1)

  for(int i=0;i<100;i++){  //100번 밥먹음
    Phil_B(R2, R3, &C2, &C3, &L1, AvailR, isSafe);
    printf("B finished eating (%d)\n",i+1);
  }
}
