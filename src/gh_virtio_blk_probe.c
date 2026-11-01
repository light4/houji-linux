#define SYS_openat 56
#define SYS_ioctl  29
#define SYS_write  64
#define SYS_exit   93
#define SYS_fork   220
#define SYS_mmap   222
#define SYS_eventfd2 19
#define SYS_pread64 67
#define SYS_nanosleep 101
#define SYS_fsync 82
#define AT_FDCWD -100
#define O_RDWR 2
#define O_RDONLY 0
static long sc(long n,long a,long b,long c,long d){register long x8 __asm__("x8")=n;register long x0 __asm__("x0")=a;register long x1 __asm__("x1")=b;register long x2 __asm__("x2")=c;register long x3 __asm__("x3")=d;__asm__ volatile("svc #0":"+r"(x0):"r"(x8),"r"(x1),"r"(x2),"r"(x3):"memory");return x0;}
static long mmap6(long a,long l,long p,long f,long fd,long o){register long x8 __asm__("x8")=SYS_mmap;register long x0 __asm__("x0")=a;register long x1 __asm__("x1")=l;register long x2 __asm__("x2")=p;register long x3 __asm__("x3")=f;register long x4 __asm__("x4")=fd;register long x5 __asm__("x5")=o;__asm__ volatile("svc #0":"+r"(x0):"r"(x8),"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5):"memory");return x0;}
static long pread64(long fd,long buf,long n,long off){register long x8 __asm__("x8")=SYS_pread64;register long x0 __asm__("x0")=fd;register long x1 __asm__("x1")=buf;register long x2 __asm__("x2")=n;register long x3 __asm__("x3")=off;__asm__ volatile("svc #0":"+r"(x0):"r"(x8),"r"(x1),"r"(x2),"r"(x3):"memory");return x0;}
static int outfd=-1; static const char outp[]="/data/local/tmp/o.txt";
static void w(const char*s,long n){ if(outfd<0) outfd=sc(SYS_openat,AT_FDCWD,(long)outp,0x241,0644); sc(SYS_write,outfd,(long)s,n,0); sc(SYS_fsync,outfd,0,0,0); }
static void hx(long v){char b[20];const char*d="0123456789abcdef";b[0]='0';b[1]='x';for(int i=0;i<16;i++)b[2+i]=d[(v>>((15-i)*4))&15];b[18]='\n';w(b,19);}
static const char dev[]="/dev/qgunyah";
static const char disk[]="/data/local/tmp/disk1.img";
static char nm[16]; static unsigned int s[8]; static unsigned char runbuf[4096];
#define LBL 16
static unsigned long base=0; static long dfd=-1; static unsigned long cap_sectors=0;
static void hex8(unsigned long v){ for(int i=7;i>=0;i--){ char c="0123456789abcdef"[(v>>(i*4))&15]; w(&c,1);} }
static unsigned int rd32(unsigned long off){ return *(unsigned int*)(base+off); }
static void wr32(unsigned long off,unsigned int v){ *(unsigned int*)(base+off)=v; }
void _start(void)
{
    const char *n="trustedvm"; int i=0; while(n[i]){nm[i]=n[i];i++;}
    if(outfd<0) outfd=sc(SYS_openat,AT_FDCWD,(long)outp,0x241,0644);
    dfd=sc(SYS_openat,AT_FDCWD,(long)disk,O_RDONLY,0);
    long sz = (dfd>0)? sc(62,dfd,0,2,0) : 0;   /* lseek SEEK_END */
    if(sz<0) sz=0; cap_sectors = sz/512;
    w("disk fd ",8); hx(dfd); w(" size ",6); hx(sz); w(" sectors ",9); hx(cap_sectors);

    long fd=sc(SYS_openat,AT_FDCWD,(long)dev,O_RDWR,0);
    long vm=sc(SYS_ioctl,fd,0xB201,0,0); w("vm ",3); hx(vm);
    if(sc(SYS_ioctl,vm,0x4010B241,(long)nm,0)){ w("setfw fail\n",11); goto out; }
    long c=sc(SYS_ioctl,vm,0xB243,0,0); w("vcpus ",6); hx(c);
    long vc[2]={-1,-1}; for(long k=0;k<c && k<2;k++) vc[k]=sc(SYS_ioctl,vm,0xB240,k,0);

    s[0]=LBL; s[1]=0;
    w("ack_reset ",10); hx(sc(SYS_ioctl,vm,0x4008B26D,(long)s,0));
    s[0]=LBL; s[1]=0; s[2]=0; s[3]=0x20;           /* VIRTIO_BLK_F_RO */
    w("feat0 ",6); hx(sc(SYS_ioctl,vm,0x4010B265,(long)s,0));
    s[2]=1; s[3]=1;                                 /* VERSION_1 */
    w("feat1 ",6); hx(sc(SYS_ioctl,vm,0x4010B265,(long)s,0));
    s[0]=LBL; s[1]=0; s[2]=0; s[3]=128;
    w("qmax ",5); hx(sc(SYS_ioctl,vm,0x4010B266,(long)s,0));
    s[0]=LBL; s[1]=8; s[2]=(unsigned)(unsigned long)&cap_sectors; s[3]=0;   /* config = capacity (8 bytes) */
    w("cfg ",4); hx(sc(SYS_ioctl,vm,0x4010B267,(long)s,0));

    long shm=0; w("shmsz_ret ",10); hx(sc(SYS_ioctl,vm,0x8008B261,(long)&shm,0));
    base=(unsigned long)mmap6(0, shm, 3, 1, vm, 0); w("mmap ",5); hx((long)base);
    long efd=sc(SYS_eventfd2,0,0,0,0); w("eventfd ",8); hx(efd);
    for (unsigned lbl = LBL; lbl <= LBL+1; lbl++) {
        s[0]=lbl; s[1]=1; s[2]=(unsigned)efd; s[3]=0;
        w("irqfd lbl ",10); hx(lbl); hx(sc(SYS_ioctl,vm,0x4010B263,(long)s,0));
        s[0]=lbl; s[1]=1; s[2]=0; s[3]=(unsigned)efd;
        w("ioevfd lbl ",11); hx(lbl); hx(sc(SYS_ioctl,vm,0x4010B262,(long)s,0));
    }

    long pid=sc(SYS_fork,0,0,0,0);
    if(pid==0){ for(int k=0;k<2 && k<c;k++) if(sc(SYS_fork,0,0,0,0)==0){ sc(SYS_ioctl,vc[k],0xB280,(long)runbuf,0); sc(SYS_exit,0,0,0,0); for(;;){} } sc(SYS_exit,0,0,0,0); for(;;){} }
    w("running\n",8);
    for(int k=0;k<10;k++){
        s[0]=LBL; s[1]=s[2]=s[3]=0;
        w("wait lbl16...\n",14);
        long e=sc(SYS_ioctl,vm,0xC010B264,(long)s,0);
        w("ev ret ",7); hx(e); w(" lbl ",5); hx(s[0]); w(" ev ",4); hx(s[1]);
        if(s[1]==8){ /* DRIVER_OK */
            unsigned int qi[4]; qi[0]=LBL; qi[1]=0; qi[2]=qi[3]=0;
            long g=sc(SYS_ioctl,vm,0xC010B269,(long)qi,0);
            w("qinfo ret ",10); hx(g); w(" num ",5); hx(qi[2]); w(" ready ",7); hx(qi[3]);
            w(" desc ",6); hex8(*(unsigned long*)&qi[4]); w("\n",1);
            /* desc/driver/device are further along the struct; re-read with full size */
            unsigned int q2[8]; for(int t=0;t<8;t++) q2[t]=0; q2[0]=LBL;
            if(sc(SYS_ioctl,vm,0xC010B269,(long)q2,0)==0){
                w("  desc ",7); hex8(*((unsigned long*)&q2[4])); w("  avail ",8); hex8(*((unsigned long*)&q2[6])); w("\n",1);
                s[0]=LBL; s[1]=0; s[2]=0; s[3]=0;
                w("ack_ok ",8); hx(sc(SYS_ioctl,vm,0x4010B26B,(long)s,0));
            }
        }
        if(s[1]==2){ w("RESET seen\n",11); }
    }
out:
    w("done\n",5);
    sc(SYS_exit,0,0,0,0); for(;;){}
}
