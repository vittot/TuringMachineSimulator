#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INC_NASTRO 512
#define INC_STATI 512
#define SIZE_COMPUT 128
#define POOL_SIZE 512
#define CONSEGNA
#ifdef CONSEGNA
    #define INPUT stdin
    #define OUTPUT stdout
#else
    #define INPUT "inputUnion.txt"
    #define OUTPUT "output.txt"
#endif

//definizione di comodo per referenziare stato in trans
typedef struct stato stato;

/***
    Transizione: stato di inizio e di partenza, carattere letto e carattere da scrivere, movimento da effettuare ('R','L' o 'S')
*/
typedef struct trans {
    stato*start;
    stato*end;
    char in;
    char out;
    char mov;
} trans;

/***
    Nodo per le liste di transizioni che sono presenti in ogni stato, per ogni char letto
*/
typedef struct nodeTrans {
    trans t;
    struct nodeTrans* next;
} nodeTrans;

/***
    Stato della TM: numero, array di liste di transizioni uscenti dallo stato, una per ognuno dei 95 char ascii stampabili (da 32 a 128), flag che indica gli stati di accettazione
*/
typedef struct stato {
    int n;
    nodeTrans* trans[95];
    int acc;
} stato;

/***
    Configurazione della macchina    ,
    I campi (*) sono puntatori per evitare la ridondanza di informazioni che sono univoche nel programma
    Il campo ptUsoNastro è un puntatore perchè il suo aggiornamento interessa tutte le istanze di tm che condividono lo stesso nastro

*/
typedef struct tm {
    char*nastro; //puntatore al nastro
    stato*stati; //puntatore all'array di stati della macchina,
    int*ptNStati; //numero di stati (*)
    int currStato; //numero dello stato corrente
    int pos; //posizizione sul nastro
    unsigned long*ptMaxMosse; //numero massimo di mosse ammesso (*)
    unsigned long nMosse; //contatore delle mosse correnti
    int dimNastro; //dimensione del nastro,
    int*ptUsoNastro; //contatore del numero di utilizzatori del nastro
} tm;

/***
    Nodo della coda bfs per l'esplorazione delle mosse della macchina, contiene la configurazione in cui ci si trova e la transizione da eseguire
*/
typedef struct nodoComput{
    tm*m;
    trans*t;
} nodoComput;

/***
    Coda per l'esplorazione bfs delle mosse della macchina. E' gestita tramite array poichè si è verificata una riduzione dei tempi rispetto alla gestione tramite lista
*/
typedef struct codaComput{
    unsigned int head;
    unsigned int tail;
    nodoComput*comput;
    unsigned int size;
} codaComput;

/***
    Nodo del pool di memoria utilizzato per allocare in modo piu' efficiente le istanze di tm da inserire nella coda bfs. Consente infatti un'allocazione iniziale di un certo numero di elementi,
    evitando il continuo richiamo al SO con singole malloc.
    Il pool ha una dimensione prefissata che non può eccedere, esaurita la quale funge solo da maschera per malloc e free, questo per evitare l'overhead della gestione di un pool espandibile,
    avendo individuato un valore iniziale che garantisce una copertura sufficiente nella maggior parte dei test
*/
typedef struct poolTmNode{
    tm m;
    struct poolTmNode*next;
}poolTmNode;

/***
    Il pool è organizzato come una lista di nodi poolTmNode, freeNode è il puntatore al primo nodo libero
    allocatedNodes è un puntatore all'indirizzo iniziale del pool all'istante della sua allocazione, da utilizzarsi per la sua free finale
*/
typedef struct poolTm{
    poolTmNode*freeNode;
    poolTmNode*allocatedNodes;
}poolTm;

/***
    Analoga struttura utilizzata per ovviare alle numerose malloc di interi per il campo ptUsoNastro della tm
*/
typedef struct poolIntNode{
    int n;
    struct poolIntNode*next;
}poolIntNode;

typedef struct poolInt{
    poolIntNode*freeNode;
    poolIntNode*allocatedNodes;
}poolInt;

//funzioni della simulazione
void inizializzaMacchina(tm*m,FILE*in);
inline static void enqueueComput(codaComput*q,tm*m,trans*t,poolTm*p);
nodoComput*dequeueComput(codaComput*q);
char runMachine(tm*m,char*str,poolInt*pi,poolTm*pm);
//funzioni per gestire il pool di memoria di tm
void loadTmPool(poolTm*p,int size);
tm* getTmAddress(poolTm*p);
void freeTmAddress(poolTm*p,tm*address);
void freeTmPool(poolTm*p);
//funzioni analoghe per gestire il pool di memoria di int
void loadIntPool(poolInt*p,int size);
int* getIntAddress(poolInt*p);
void freeIntAddress(poolInt*p,int*address);
void freeIntPool(poolInt*p);

int main()
{
    FILE*f,*in;
    tm m;
    char res;
    int i,j;
    nodeTrans*toFreeTrans;
    poolInt pi;
    poolTm pm;
    //inizializzazione pool di memoria
    pm.freeNode=NULL;
    pm.allocatedNodes=NULL;
    pi.freeNode=NULL;
    pi.allocatedNodes=NULL;
    loadTmPool(&pm,POOL_SIZE);
    loadIntPool(&pi,POOL_SIZE);
    //fine inizializzazione pool di memoria
    #ifndef CONSEGNA
        f=fopen(OUTPUT,"w");
        in=fopen(INPUT,"r");
    #else
        f=stdout;
        in=stdin;
    #endif
    inizializzaMacchina(&m,in);
    char*str;
    do
    {
        //alloca automaticamente lo spazio necessario in str
       fscanf(in,"%ms",&str);
       if(str!=NULL)
       {
            if(strcmp(str,"")!=0)
            {
                res=runMachine(&m, str,&pi,&pm);
                fprintf(f,"%c\n",res);
            }
            free(str);
       }
    }while(str!=NULL);
    //libera tutta la memoria dinamica ancora allocata
    free(m.ptMaxMosse);
    for(i=*(m.ptNStati)-1;i>=0;i--)
        for(j=94;j>=0;j--)
            while(m.stati[i].trans[j] != NULL)
            {
                toFreeTrans = m.stati[i].trans[j];
                m.stati[i].trans[j] = m.stati[i].trans[j]->next;
                free(toFreeTrans);
            }
    free(m.stati);
    freeTmPool(&pm);
    freeIntPool(&pi);
    #ifndef CONSEGNA
        fclose(f);
        fclose(in);
    #endif

    return 0;
}

/***
    Esegue il core del programma, elaborando una stringa e restituendo il risultato
    L'esplorazione delle mosse della NTM è di tipo bfs.
    @param tm*m - configurazione della macchina da simulare
    @param char*str - stringa da elaborare
    @param poolInt*pi - pool di int a cui attingere
    @param poolTm*pm - pool di tm a cui attingere
    @return char '1','0' o 'U' come da specifiche
*/
char runMachine(tm*m,char*str,poolInt*pi,poolTm*pm)
{
    int len = strlen(str);
    int U=0; //flag che marca il raggiungimento del numero max di mosse consentito
    m->nastro = malloc(sizeof(char)*(len));
    memcpy(m->nastro,str,strlen(str));
    m->pos = 0;
    m->currStato = 0;
    m->nMosse = 0;
    m->dimNastro = len;
    //m->ptUsoNastro=malloc(sizeof(int));
    m->ptUsoNastro=getIntAddress(pi);
    *(m->ptUsoNastro)=1;

    //variabili varie di appoggio
    tm*macchina;
    int*ptUsoNastro;
    char*ptNastro;
    char*newNastro;
    codaComput q;
    nodeTrans*nt;
    trans*t;
    nodoComput*nc;

    //init coda bfs
    q.head = 0;
    q.tail = 0;
    q.comput = malloc(sizeof(nodoComput)*SIZE_COMPUT);
    q.size = SIZE_COMPUT-1;

    //carica le prime transizioni
    nt = m->stati[m->currStato].trans[m->nastro[m->pos]-32];
    if(nt==NULL)
        return '0';

    do{
        enqueueComput(&q,m,&(nt->t),pm);
        *(m->ptUsoNastro) += 1;
        nt=nt->next;
    }while(nt!=NULL);

    while(q.head != q.tail) //finche' la coda bfs non è vuota
    {
        nc= dequeueComput(&q);
        t=nc->t;
        m=(nc->m);
        ///RILEVA LOOP
        if((t->in=='_' && t->mov == 'R' && t->start == t->end && m->pos==m->dimNastro-1 ) || (t->in=='_' && t->mov == 'L' && t->start == t->end && m->pos==0 ) || (t->in==t->out && t->mov=='S' && t->start==t->end) )
            m->nMosse = *(m->ptMaxMosse);

        ///GESTIONE SCRITTURA
        if(t->in != t->out)
        {
            newNastro = malloc(sizeof(char)*m->dimNastro);
            memcpy(newNastro,m->nastro, m->dimNastro*sizeof(char));
            if(*(m->ptUsoNastro)==1)
                free(m->nastro);
            else
            {
                *(m->ptUsoNastro) -= 1;
                //m->ptUsoNastro=malloc(sizeof(int));
                m->ptUsoNastro = getIntAddress(pi);
                *(m->ptUsoNastro)=1;
            }
            m->nastro = newNastro;
            m->nastro[m->pos] = t->out;
        }
        ///GESTIONE MOVIMENTO
        if(t->mov == 'R')
        {
            m->pos++;
            if(m->pos == m->dimNastro)
            {
                 newNastro = malloc(sizeof(char)*m->dimNastro+INC_NASTRO);
                 memcpy(newNastro,m->nastro,m->dimNastro);
                 if(*(m->ptUsoNastro)==1)
                    free(m->nastro);
                 else
                 {
                    *(m->ptUsoNastro) -= 1;
                    //m->ptUsoNastro=malloc(sizeof(int));
                    m->ptUsoNastro = getIntAddress(pi);
                    *(m->ptUsoNastro)=1;
                 }
                 m->nastro = newNastro;

                 memset(m->nastro+m->dimNastro,'_',INC_NASTRO);
                 m->dimNastro += INC_NASTRO;
            }

        }
        else if(t->mov == 'L')
        {
            m->pos--;
            if(m->pos < 0)
            {
                newNastro = malloc(sizeof(char)*m->dimNastro+INC_NASTRO);
                memcpy(newNastro+INC_NASTRO, m->nastro, m->dimNastro);
                m->dimNastro += INC_NASTRO;
                m->pos += INC_NASTRO;
                if(*(m->ptUsoNastro)==1)
                    free(m->nastro);
                else
                {
                    *(m->ptUsoNastro) -= 1;
                    //m->ptUsoNastro=malloc(sizeof(int));
                    m->ptUsoNastro = getIntAddress(pi);
                    *(m->ptUsoNastro)=1;
                }

                m->nastro=newNastro;
                memset(m->nastro,'_',INC_NASTRO);
            }

        }
        ///STATO
        m->currStato = t->end->n;
        m->nMosse++;

        if(t->end->acc){ //arrivo in uno stato di accettazione
            free(m->nastro);
            //free(m->ptUsoNastro);
            freeIntAddress(pi, m->ptUsoNastro);
            free(q.comput);

            return '1';
        }
        ///ACCODO TRANSIZIONI SUCCESSIVE
        ptUsoNastro = m->ptUsoNastro;
        ptNastro = m->nastro;
        if(m->nMosse<*(m->ptMaxMosse))
        {
            macchina=m;
            nt = m->stati[m->currStato].trans[m->nastro[m->pos]-32];
            while(nt!=NULL)
            {
                enqueueComput(&q,macchina,&(nt->t),pm);
                *(ptUsoNastro) += 1;
                nt=nt->next;
            }
        }
        else
            U=1;
        *(ptUsoNastro) -=1;
        if(*(ptUsoNastro)==0)
        {
            free(ptNastro);
            //free(ptUsoNastro);
            freeIntAddress(pi, ptUsoNastro);
        }
        freeTmAddress(pm,m);
    }
    free(q.comput);
    if(U)
        return 'U';

    return '0';
}

/***
    Carica tutti parametri della macchina dall'input: transizioni, stati di accettazione, numero massimo di mosse
    @param tm*m - struttura dati in cui caricare i parametri letti
    @param FILE*in - stream da cui leggere l'input
*/
void inizializzaMacchina(tm*m,FILE*in)
{
    char*row = malloc(sizeof(char)*50); //buffer di per la lettura delle righe di input
    nodeTrans*nt;
    int nStatiAllocati, statoP, statoA, maxStato, statoAcc, i;

    m->nastro = NULL;
    m->ptNStati = malloc(sizeof(int));
    *(m->ptNStati) = 0;
    //alloca un numero base di stati
    m->stati = malloc(INC_STATI*sizeof(stato));
    nStatiAllocati = INC_STATI;
    for(i=nStatiAllocati-INC_STATI;i<nStatiAllocati;i++)
    {
        m->stati[i].n = i;
        memset(m->stati[i].trans,0,95*sizeof(nodeTrans*));
        m->stati[i].acc = 0;
    }

    fgets(row, 50, in); //leggo "tr"

    //leggo le transizioni
    fgets(row, 50, in);

    while(strcmp(row,"acc\n")!=0)
    {
        nt = malloc(sizeof(nodeTrans));
        sscanf(row,"%d %c %c %c %d", &statoP, &(nt->t.in), &(nt->t.out), &(nt->t.mov), &statoA);
        maxStato = (statoP>statoA) ? statoP : statoA;
        //incrementa il numero di stati allocati, se necessario
        while(maxStato>=nStatiAllocati)
        {
            nStatiAllocati += INC_STATI;
            m->stati = realloc(m->stati, nStatiAllocati * sizeof(stato));
            for(i=nStatiAllocati-INC_STATI;i<nStatiAllocati;i++)
            {
                m->stati[i].n = i;
                memset(m->stati[i].trans,0,95*sizeof(nodeTrans*));
                m->stati[i].acc = 0;
            }
        }
        if(maxStato > *(m->ptNStati))
        {
            *(m->ptNStati) = maxStato;
        }
        nt->t.start = &(m->stati[statoP]);
        nt->t.end = &(m->stati[statoA]);
        //aggiungo la transizione alla lista di quelle uscenti dallo stato di partenza, aventi il suo char come valore letto
        nt->next = m->stati[statoP].trans[nt->t.in-32];
        m->stati[statoP].trans[nt->t.in-32] = nt;

        fgets(row, 50, in);
    }
    //leggo gli stati di accettazione
    fgets(row, 50, in);
    while(strcmp(row,"max\n")!=0)
    {
        sscanf(row, "%d",&statoAcc);
        m->stati[statoAcc].acc = 1;
        fgets(row, 50, in);
    }

    //leggo il num max di mosse
    m->ptMaxMosse = malloc(sizeof(unsigned long int));
    fscanf(in, "%lu ", m->ptMaxMosse);

    fgets(row, 50, in); //leggo "run"
    free(row);

}

/***
    Aggiunge un nodo di computazione alla coda bfs
    E' inline static per suggerire al compilatore di renderla inline; dato l'elevato numero di chiamate questa ottimizzazione produce effetti sensibili sul tempo di esecuzione del programma
    @param codaComput*q - coda bfs a cui aggiungere il nodo di computazione
    @param tm*m - configurazione della macchina per il nodo di computazione
    @param trans*t - transizione del nodo di computazione
    @param poolTm*p - pool di memoria da cui attingere per l' "allocazione" della configurazione nel nodo di computazione
*/
inline static void enqueueComput(codaComput*q,tm*m,trans*t,poolTm*p)
{
    int oldSize;
    if(q->head == q->tail + 1 || (q->head == 0 && q->tail == q->size-1)) //coda piena
    {
        oldSize = q->size;
        q->size = ((q->size+1) << 1) -1;
        q->comput = realloc(q->comput,(q->size+1) * sizeof(nodoComput));
        //ridispone la coda come array circolare in modo corretto
        if(q->tail < q->head)
        {
            memcpy(q->comput+oldSize,q->comput,(q->tail+1)*sizeof(nodoComput));
            q->tail += oldSize;
        }
    }
    q->comput[q->tail].m = getTmAddress(p);
    //q->comput[q->tail].m = malloc(sizeof(tm));
    *(q->comput[q->tail].m) = *m;
    q->comput[q->tail].t = t;
    q->tail++;
    if(q->tail == q->size)
        q->tail = 0;

}

/***
    Ritorna il nodo di computazione in testa alla coda
    @param codaComput*q - coda bfs da cui prelevare il nodo
    @return nodoComput*q - indirizzo del nodo in testa alla coda
*/
nodoComput*dequeueComput(codaComput*q)
{
    nodoComput*nc = &(q->comput[q->head]);
    q->head++;
    if(q->head == q->size)
        q->head = 0;
    return nc;
}

/***
  Esegue l'allocazione iniziale del pool di memoria per le tm
  @param poolTm*p - pool di cui deve eseguire l'allocazione
  @param int size - numero di elementi da allocare
*/
void loadTmPool(poolTm*p,int size)
{
    int i;
    poolTmNode*poolNodes=malloc(sizeof(poolTmNode)*size);
    p->allocatedNodes=poolNodes;
    poolTmNode*addr=poolNodes;
    for(i=0;i<size;i++)
    {
        addr->next  = (p->freeNode);
        p->freeNode = addr;
        addr++;
    }
}

/***
    Ritorna un indirizzo di memoria per tm dal pool; se il pool è pieno esegue una malloc e ritorna l'indirizzo allocato
    @param poolTm*p - pool da cui attingere
    @return tm* - indirizzo per l'allocazione di una tm
*/
tm* getTmAddress(poolTm*p)
{
    if(p->freeNode == NULL){
        return malloc(sizeof(tm));
    }

    tm*addr = (tm*)(p->freeNode);
    p->freeNode = (p->freeNode->next);
    return addr;
}

/***
    Libera un indirizzo per tm precedentemente allocato. Se appartiene al pool ritorna ad esso, altrimenti viene eseguita una free
    @param poolTm*p - pool in cui eventualmente reinserirlo
    @param tm*address - indirizzo da liberare
*/
void freeTmAddress(poolTm*p,tm*address)
{
    poolTmNode*pn=(poolTmNode*)(address);
    if(pn>=p->allocatedNodes && pn <= p->allocatedNodes+POOL_SIZE)
    {
        pn->next = p->freeNode;
        p->freeNode = pn;
    }
    else
        free(address);
}

/***
  Esegue l'allocazione iniziale del pool di memoria int
  @param poolInt*p - pool di cui deve eseguire l'allocazione
  @param int size - numero di elementi da allocare
*/
void loadIntPool(poolInt*p,int size)
{
    int i;
    poolIntNode*poolNodes=malloc(sizeof(poolIntNode)*size);
    p->allocatedNodes=poolNodes;
    poolIntNode*addr=poolNodes;
    for(i=0;i<size;i++)
    {
        addr->next  = (p->freeNode);
        p->freeNode = addr;
        addr++;
    }
}

/***
    Ritorna un indirizzo di memoria per int dal pool; se il pool è pieno esegue una malloc e ritorna l'indirizzo allocato
    @param poolInt*p - pool da cui attingere
    @return int* - indirizzo per l'allocazione di un int
*/
int* getIntAddress(poolInt*p)
{
    if(p->freeNode == NULL){
         return malloc(sizeof(int));
    }

    int*addr = (int*)(p->freeNode);
    p->freeNode = (p->freeNode->next);
    return addr;
}

/***
    Libera un indirizzo per int precedentemente allocato. Se appartiene al pool ritorna ad esso, altrimenti viene eseguita una free
    @param poolInt*p - pool in cui eventualmente reinserirlo
    @param int*address - indirizzo da liberare
*/
void freeIntAddress(poolInt*p,int*address)
{
    poolIntNode*pn=(poolIntNode*)(address);
    if(pn>=p->allocatedNodes && pn < p->allocatedNodes+POOL_SIZE)
    {
        pn->next = p->freeNode;
        p->freeNode = pn;
    }
    else
        free(address);
}

/***
    Libera il pool di int allocato
    @param poolInt*p - pool da liberare
*/
void freeIntPool(poolInt*p)
{
    free(p->allocatedNodes);
}

/***
    Libera il pool di tm allocato
    @param poolTm*p - pool da liberare
*/
void freeTmPool(poolTm*p)
{
    free(p->allocatedNodes);
}
