#include <stdio.h>
#include <pcap.h>
#include <malloc.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "protocol.h"

#define IPTOSBUFFERS    12
int tstamp_offset;
int offset_flag = 0;
int cycle = 1;

//int��ʾ��ipת�ɵ��ʽ��ʾ
static char *iptos(bpf_u_int32 in)
{
    static char output[IPTOSBUFFERS][3*4+3+1];
    static short which;
    u_char *p;

    p = (u_char *)&in;
    which = (which + 1 == IPTOSBUFFERS ? 0 : which + 1);
    sprintf(output[which], "%d.%d.%d.%d", p[3], p[2], p[1], p[0]);
    return output[which];
}

//ʱ������Ի�չʾ
char *long2time(long ltime)
{
    time_t t;
    struct tm *p;
    static char s[100];

    t = ltime;
    p = gmtime(&t);

    strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", p);
    return s;
}

// ��Ҫ��������һ����ϣ�������洦������״̬�İ�
// ����������ֱ𱣴�tcp��udp������
//tcp����ͷ�ڵ�
net_link_header *FLowLink_TCP;
//udp����ͷ�ڵ�
net_link_header *FLowLink_UDP;

/* ========== hash table ============= */
#define HASH_TABLE_SIZE 0xffff
//����һ����ϣ��
p_net_link HashTable[HASH_TABLE_SIZE];

//��ʼ����������ͳ����Ϣ�Լ���һ���ڵ����Ϊ��
void init_flowLink(net_link_header *head)
{
    head->count_conn        = 0;
    head->count_upl_pkt     = 0;
    head->count_downl_pkt   = 0;
    head->count_upl         = 0;
    head->count_downl       = 0;
    head->link              = NULL;
}

//���һ����㵽ָ��������������Ӧ������ͷ��Ϣ��ѭ�������ڵ����λ����ͷ�ڵ����һ��
void add_to_flowLink(net_link_header *head, const net_link_node *theNode)
{
    net_link_node *newNode = (net_link_node *)malloc(sizeof(net_link_node));
    memcpy(newNode, theNode, sizeof(net_link_node));

    //����ӵ������ͷͳ����Ϣ��Ӧ����
    head->count_conn ++;
    head->count_upl_pkt     += newNode->nln_upl_pkt;
    head->count_downl_pkt   += newNode->nln_downl_pkt;
    head->count_upl         += newNode->nln_upl_size;
    head->count_downl       += newNode->nln_downl_size;

    newNode->next = head->link;
    head->link = newNode;
}

//�����ӵ�ͷ�ڵ����ͨ�ڵ����
void clear_flowLink(net_link_header *head)
{
    if( head->link == NULL ){ return;}

    net_link_node *pTemp1 = NULL;
    net_link_node *pTemp2 = NULL;

    pTemp1 = head->link;
    pTemp2 = pTemp1->next;
    //����ͷ���ͨ�ڵ���ڴ�
    while( pTemp2 != NULL )
    {
        free(pTemp1);
        pTemp1 = pTemp2;
        pTemp2 = pTemp1->next;
    }
    free(pTemp1);

    head->link = NULL;
}

//����tcp��������ǰtcp�����ϵ����нڵ���Ϣд���ļ���ȥ������������tcp����
void parse_flowLink_TCP(FILE *fOutput)
{
    //���ļ���д���ܵĸ���ͳ����Ϣ
    printf("TCP���Ӹ�����\t%d\n", FLowLink_TCP->count_conn);
    printf("TCP���ݰ�������\t%d\n", FLowLink_TCP->count_upl_pkt + FLowLink_TCP->count_upl_pkt);
    printf("TCP������������\t%d bytes\n", FLowLink_TCP->count_upl + FLowLink_TCP->count_downl);
    printf("TCP�����ϴ�����\t%d bytes\n", FLowLink_TCP->count_upl);
    printf("TCP������������\t%d bytes\n", FLowLink_TCP->count_downl);
    printf("-----------------------\n");

    fprintf(fOutput, "TCP���Ӹ�����\t%d\n", FLowLink_TCP->count_conn);
    fprintf(fOutput, "TCP���ݰ�������\t%d\n", FLowLink_TCP->count_upl_pkt + FLowLink_TCP->count_upl_pkt);
    fprintf(fOutput, "TCP������������\t%d bytes\n", FLowLink_TCP->count_upl + FLowLink_TCP->count_downl);
    fprintf(fOutput, "TCP�����ϴ�����\t%d bytes\n", FLowLink_TCP->count_upl);
    fprintf(fOutput, "TCP������������\t%d bytes\n", FLowLink_TCP->count_downl);
    fprintf(fOutput, "-----------------------\n");

    net_link_node *pTemp = NULL;
    pTemp = FLowLink_TCP->link;
    //д��ÿ�����ݰ�����Ϣ
    while( pTemp != NULL )
    {
        printf("%s:%u\t", iptos(pTemp->nln_5set.sip), pTemp->nln_5set.sport);
        printf("==>\t%s:%u\t", iptos(pTemp->nln_5set.dip), pTemp->nln_5set.dport);
        printf("�ϴ���������%d\t", pTemp->nln_upl_pkt);
        printf("���ذ�������%d\t", pTemp->nln_downl_pkt);
        printf("�ϴ���������%d bytes\t", pTemp->nln_upl_size);
        printf("������������%d bytes\t", pTemp->nln_downl_size);
        printf("\n");

        fprintf(fOutput, "%s:%u\t", iptos(pTemp->nln_5set.sip), pTemp->nln_5set.sport);
        fprintf(fOutput, "==>\t%s:%u\t", iptos(pTemp->nln_5set.dip), pTemp->nln_5set.dport);
        fprintf(fOutput, "�ϴ���������%d\t", pTemp->nln_upl_pkt);
        fprintf(fOutput, "���ذ�������%d\t", pTemp->nln_downl_pkt);
        fprintf(fOutput, "�ϴ���������%d bytes\t", pTemp->nln_upl_size);
        fprintf(fOutput, "������������%d bytes\t", pTemp->nln_downl_size);
        fprintf(fOutput, "\n");
        pTemp = pTemp->next;
    }
    //������֮�����tcp����
    clear_flowLink(FLowLink_TCP);
}

//����udp��������ǰudp�����еĽڵ���Ϣд�뵽�ļ���ȥ������������udp����
void parse_flowLink_UDP(FILE *fOutput)
{
    //���ļ���д��udp���ݰ���ͳ����Ϣ
    printf("UDP���Ӹ�����\t%d\n", FLowLink_UDP->count_conn);
    printf("UDP���ݰ�������\t%d\n", FLowLink_UDP->count_upl_pkt + FLowLink_UDP->count_upl_pkt);
    printf("UDP����������\t%d bytes\n", FLowLink_UDP->count_upl + FLowLink_UDP->count_downl);
    printf("UDP�����ϴ�����\t%d bytes\n", FLowLink_UDP->count_upl);
    printf("UDP������������\t%d bytes\n", FLowLink_UDP->count_downl);

    fprintf(fOutput, "UDP���Ӹ�����\t%d\n", FLowLink_UDP->count_conn);
    fprintf(fOutput, "UDP���ݰ�������\t%d\n", FLowLink_UDP->count_upl_pkt + FLowLink_UDP->count_upl_pkt);
    fprintf(fOutput, "UDP����������\t%d bytes\n", FLowLink_UDP->count_upl + FLowLink_UDP->count_downl);
    fprintf(fOutput, "UDP�����ϴ�����\t%d bytes\n", FLowLink_UDP->count_upl);
    fprintf(fOutput, "UDP������������\t%d bytes\n", FLowLink_UDP->count_downl);
    //������֮�����udp����
    clear_flowLink(FLowLink_UDP);
}

//����һ���ֽں͵ڶ����ֽڽ���λ��,ntohs()??
u_short get_ushort_net(u_short virtu)
{
    return (u_short)(virtu >> 8 | virtu << 8);
}

//��ȡ5Ԫ��Ĺ�ϣֵ
u_short get_hash(const net5set *theSet)
{
    u_int srcIP = theSet->sip;
    u_int desIP = theSet->dip;
    u_int port  = (u_int)(theSet->sport * theSet->dport);
    u_int res   = (srcIP^desIP)^port;
    u_short hash= (u_short)((res & 0x00ff)^(res >> 16));
    return hash;
}

/* ����״̬���ݰ� */
void add_to_hashTable(u_short hash, const net_link_node *theNode, u_char flags)
{
    net_link_node *HashNode = (net_link_node *)malloc(sizeof(net_link_node));
    memcpy(HashNode, theNode, sizeof(net_link_node));

    if(HashTable[hash] == NULL) // �жϵ�ǰ HASH �ؼ����Ƿ��Ѵ��ڣ������ھͽ���ǰ�ڵ���� HASH ����
    {
        HashTable[hash] = HashNode;
        return;
    }
    net_link_node *pTemp = HashTable[hash];
    net_link_node *pBack = NULL;
    int isSame_up = 0; // ͬһ�ϴ�����
    int isSame_down = 0; // ͬһ��������
    while(pTemp != NULL)
    {
        /* IP ��ַ�Ͷ˿�ƥ�� */
        isSame_up = (pTemp->nln_5set.sip == HashNode->nln_5set.sip)
        && (pTemp->nln_5set.dip == HashNode->nln_5set.dip)
        && (pTemp->nln_5set.sport == HashNode->nln_5set.sport)
        && (pTemp->nln_5set.dport == HashNode->nln_5set.dport);

        isSame_down = (pTemp->nln_5set.dip == HashNode->nln_5set.sip)
        && (pTemp->nln_5set.sip == HashNode->nln_5set.dip)
        && (pTemp->nln_5set.dport == HashNode->nln_5set.sport)
        && (pTemp->nln_5set.sport == HashNode->nln_5set.dport);

        if( isSame_up )
        {
            pTemp->nln_upl_size += HashNode->nln_upl_size;
            pTemp->nln_upl_pkt ++;
            if(pTemp->nln_status == ESTABLISHED && (flags & TH_FIN) )
            {
                pTemp->nln_status = FIN_WAIT_1;
                HashNode->nln_status = FIN_WAIT_1;
            }
            else if (pTemp->nln_status == FIN_WAIT_2 && (flags & TH_ACK))
            {
                pTemp->nln_status = TIME_WAIT;
                HashNode->nln_status = TIME_WAIT;
            }
            //��һ��tcp����
            else if (pTemp->nln_status == TIME_WAIT && (flags & TH_ACK))
            {
                //���Ӷ���
                pTemp->nln_status = CLOSED;
                if(pBack == NULL)
                {
                    HashTable[hash] = NULL;
                }
                else
                {
                    pBack->next = pTemp->next;
                }
                add_to_flowLink(FLowLink_TCP, pTemp);
                free(pTemp);
            }
            else if(pTemp->nln_status == CLOSE_WAIT && (flags & TH_FIN))
            {
                pTemp->nln_status = LAST_ACK;
                HashNode->nln_status = LAST_ACK;
            }
            free(HashNode);
            break;
        }
        else if( isSame_down )
        {
            pTemp->nln_downl_size += HashNode->nln_upl_size;
            pTemp->nln_downl_pkt ++;
            if(pTemp->nln_status == ESTABLISHED && (flags & TH_FIN))
            {
                pTemp->nln_status = CLOSE_WAIT;
                HashNode->nln_status = FIN_WAIT_1;
            }
            else if(pTemp->nln_status == LAST_ACK && (flags & TH_ACK))
            {
                pTemp->nln_status = CLOSED;
                HashNode->nln_status = TIME_WAIT;
                if(pBack == NULL)
                {
                    HashTable[hash] = NULL;
                }
                else
                {
                    pBack->next = pTemp->next;
                }
                add_to_flowLink(FLowLink_TCP, pTemp);
                free(pTemp);
            }
            else if(pTemp->nln_status == FIN_WAIT_1 && (flags & TH_ACK))
            {
                pTemp->nln_status = FIN_WAIT_2;
                HashNode->nln_status = CLOSE_WAIT;
            }

            free(HashNode);
            break;
        }
        pBack = pTemp;
        pTemp = pTemp->next;
    }
    if(pTemp == NULL)
    {
        pBack->next = HashNode;
    }
}
//��չ�ϣ������ϣ��ÿ����λ�����нڵ���ӵ�tcp������
void clear_hashTable()
{
    int i = 0;
    net_link_node *pTemp1 = NULL;
    net_link_node *pTemp2 = NULL;
    //������ϣ������ϣ��ÿ����λ�����нڵ���ӵ�tcp������
    for(i = 0; i < HASH_TABLE_SIZE; i++)
    {
        if(HashTable[i] == NULL){ continue;}

        pTemp1 = HashTable[i];
        while(pTemp1 != NULL)
        {
            pTemp2 = pTemp1->next;
            add_to_flowLink(FLowLink_TCP, pTemp1);
            free(pTemp1);
            pTemp1 = pTemp2;
        }
        HashTable[i] = NULL;
    }
}

void display_binary(u_short number,int* ret) {
    int i = 0;
    int count = 0;
    int result[16];
    while (number > 0) {
        result[i++] = number % 2;
        number /= 2;
    }
    int j = 16-i;
    for (;j > 0; j--) {
         ret[count++] = 0;
    }
    for (; i>0; i--) {
        ret[count++] = result[i-1];
    }
}

void cb_getPacket(u_char *dumpfile, const struct pcap_pkthdr *header, const char *packet)
{
    FILE *log = fopen("log.txt", "a+");
    FILE *fOutput = fopen("result.data", "a+");
    static int id = 0;
    // ip_header *seg_ip = (ip_header*)(package + ETHER_LEN);
    pcap_dump(dumpfile, header, packet);
    //5Ԫ����Ϣ
    net5set         *Cur5Set    = (net5set *)malloc(sizeof(net5set));
    //��ͨ����ڵ�
    net_link_node   *LinkNode   = (net_link_node *)malloc(sizeof(net_link_node));
    time_t capture_time;
    struct tm *time;
    char nowtime[24];
    capture_time = header->ts.tv_sec;
    if (offset_flag == 0) {
        tstamp_offset = capture_time;
        offset_flag = 1;
    }
    time = localtime(&capture_time);
    memset(nowtime, 0, sizeof(nowtime));
    strftime(nowtime, 24, "%Y-%m-%d %H:%M:%S", time);
    if(capture_time - tstamp_offset >= cycle) {
        printf(">>>>> ʱ��Σ�%s", long2time(tstamp_offset));
        printf(" --> %s\n", long2time(tstamp_offset + cycle));

        printf("----------------------\n");

        fprintf(fOutput, ">>>>> ʱ��Σ�%s", long2time(tstamp_offset));
        fprintf(fOutput, " --> %s\n", long2time(tstamp_offset + cycle));

        fprintf(fOutput, "----------------------\n");
        clear_hashTable();
        parse_flowLink_UDP(fOutput);
        init_flowLink(FLowLink_UDP);
        printf("----------------------\n");
        fprintf(fOutput, "----------------------\n");
        //����TCP����
        parse_flowLink_TCP(fOutput);
        //������TCP�����������TCP����û��Ҫ����
        init_flowLink(FLowLink_TCP);
        tstamp_offset = capture_time;
        printf("========================\n");
    }
    printf("[%d]�����ݰ�:%s(%d bytes)\n", ++id, nowtime, header->len);
    fprintf(log, "[%d]�����ݰ�:%s(%d bytes)\n", id, nowtime, header->len);
    ether_header *ether;
    ether = (ether_header *)(packet);
    printf("��·��(MAC):%.2x:%.2x:%.2x:%.2x:%.2x:%.2x -> %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
           ether->host_src[0],
           ether->host_src[1],
           ether->host_src[2],
           ether->host_src[3],
           ether->host_src[4],
           ether->host_src[5],
           ether->host_dest[0],
           ether->host_dest[1],
           ether->host_dest[2],
           ether->host_dest[3],
           ether->host_dest[4],
           ether->host_dest[5]
    );
    fprintf(log, "��·��(MAC):%.2x:%.2x:%.2x:%.2x:%.2x:%.2x -> %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
           ether->host_src[0],
           ether->host_src[1],
           ether->host_src[2],
           ether->host_src[3],
           ether->host_src[4],
           ether->host_src[5],
           ether->host_dest[0],
           ether->host_dest[1],
           ether->host_dest[2],
           ether->host_dest[3],
           ether->host_dest[4],
           ether->host_dest[5]);
    if (htons(ether->type) == ETHER_TYPE_IP) {
        unsigned int protocol;
        unsigned int ip_head_len;
        unsigned int ip_total_len;
        ip_header *ip_packet = (ip_header*)(packet + ETHER_LEN);
        int sip = (((int)ip_packet->src_ip_address.byte4)) + (((int)ip_packet->src_ip_address.byte3)<<8) + (((int)ip_packet->src_ip_address.byte2)<<16) + (((int)ip_packet->src_ip_address.byte1)<<24);
        Cur5Set->sip = sip;
        int dip =  (((int)ip_packet->dest_ip_address.byte4)) + (((int)ip_packet->dest_ip_address.byte3)<<8) + (((int)ip_packet->dest_ip_address.byte2)<<16)+ (((int)ip_packet->dest_ip_address.byte1)<<24);
        Cur5Set->dip = dip;
        Cur5Set->protocol = ip_packet->proto;
        protocol = *((unsigned int *)(&ip_packet->proto)) & 0xff;
        //IPͷ������
        ip_head_len = (ip_packet->ver_ihl & 0xf)*4;
        //IP�ܳ���
        ip_total_len = ntohs(ip_packet->tlen);
        //���ݰ�����
        unsigned int packet_len = ip_total_len - ip_head_len;
         //ԴIP
        printf("\n�����(IPЭ��),ͷ����(%d bytes),���ݳ���(%d bytes):", ip_head_len, packet_len);
        fprintf(log, "\n�����(IPЭ��),ͷ����(%d bytes),���ݳ���(%d bytes):", ip_head_len, packet_len);
        printf("%d.%d.%d.%d -> %d.%d.%d.%d\n",
            ip_packet->src_ip_address.byte1,
            ip_packet->src_ip_address.byte2,
            ip_packet->src_ip_address.byte3,
            ip_packet->src_ip_address.byte4,
            ip_packet->dest_ip_address.byte1,
            ip_packet->dest_ip_address.byte2,
            ip_packet->dest_ip_address.byte3,
            ip_packet->dest_ip_address.byte4);
        fprintf(log, "%d.%d.%d.%d -> %d.%d.%d.%d\n",
            ip_packet->src_ip_address.byte1,
            ip_packet->src_ip_address.byte2,
            ip_packet->src_ip_address.byte3,
            ip_packet->src_ip_address.byte4,
            ip_packet->dest_ip_address.byte1,
            ip_packet->dest_ip_address.byte2,
            ip_packet->dest_ip_address.byte3,
            ip_packet->dest_ip_address.byte4);
        unsigned short src_port, dest_port;
        //���ݳ���
        unsigned int data_len;
        if (protocol == IP_TCP) {
            tcp_header *tcp_packet;
            unsigned int ack = 0;
            unsigned int sequence = 0;
            u_char flag;
            char flag_str[100];
            tcp_packet = (tcp_header *) ((unsigned char *)ip_packet + ip_head_len);
            src_port = ntohs(tcp_packet->th_sport);
            Cur5Set->sport = src_port;
            dest_port = ntohs(tcp_packet->th_dport);
            Cur5Set->dport = dest_port;
            ack = ntohl(tcp_packet->th_ack);
            sequence = ntohl(tcp_packet->th_seq);
            unsigned short hrf = tcp_packet->th_len_x2;
            //unsigned int tcp_head_len = ((hrf & 0xf000) >> 12)*4;
            unsigned int tcp_head_len = (hrf>>4) * 4;
            data_len = packet_len - tcp_head_len;
            //Ϊ�˼���udp���ģ�Ĭ�ϸ����ݰ����������ݰ�������״̬λΪESTABLISHED
            LinkNode->nln_5set      = *Cur5Set;
            LinkNode->nln_status    = ESTABLISHED;
            LinkNode->next          = NULL;
            if (strcmp(iptos(sip), "192.168.1.108") == 0) {
                LinkNode->nln_upl_size  = data_len;
                LinkNode->nln_downl_size= 0;
                LinkNode->nln_upl_pkt   = 1;
                LinkNode->nln_downl_pkt = 0;
            } else {
                LinkNode->nln_upl_size  = 0;
                LinkNode->nln_downl_size= data_len;
                LinkNode->nln_upl_pkt   = 0;
                LinkNode->nln_downl_pkt = 1;
            }
            add_to_hashTable(get_hash(Cur5Set), LinkNode, tcp_packet->th_flags);
            flag = tcp_packet->th_flags;
            strcpy(flag_str, "");
            if((flag & TH_ACK) == TH_ACK){
                strcat(flag_str, "ACK=1 ");
            }else{
                strcat(flag_str, "ACK=0 ");
            }

            if((flag & TH_SYN) == TH_SYN){
                strcat(flag_str, "SYN=1 ");
            }else{
                strcat(flag_str, "SYN=0 ");
            }

            if((flag & TH_FIN) == TH_FIN){
                strcat(flag_str, "FIN=1");
            }else{
                strcat(flag_str, "FIN=0");
            }
            printf("�����(TCP):Դ�˿ں�:%hu -> Ŀ�Ķ˿ں�:%hu, ���ݳ���:%d, ack:%u, sequence:%u, ��־:%s\n", src_port, dest_port, data_len, ack, sequence, flag_str);
            fprintf(log, "�����(TCP):Դ�˿ں�:%hu -> Ŀ�Ķ˿ں�:%hu, ���ݳ���:%d, ack:%u, sequence:%u, ��־:%s\n", src_port, dest_port, data_len, ack, sequence, flag_str);
            if ((data_len > 0) && (src_port == 80 || dest_port == 80)) {
                char *data = (char *)tcp_packet + tcp_head_len;
                char* http_text = (char*)malloc(sizeof(char)*data_len);
                strncpy(http_text, data, data_len);
                printf("Ӧ�ò㱨��(HTTP):\n%s\n:", http_text);
                fprintf(log, "Ӧ�ò㱨��(HTTP):\n%s\n:", http_text);
            }
        } else if (protocol == IP_UDP) {
            udp_header* udp_packet;
            udp_packet = (udp_header*) ((unsigned char *)ip_packet + ip_head_len);
            data_len = packet_len - UDP_LEN;
            src_port = ntohs(udp_packet->uh_sport);
            Cur5Set->sport = src_port;
            dest_port = ntohs(udp_packet->uh_dport);
            Cur5Set->dport = dest_port;
            //Ϊ�˼���udp���ģ�Ĭ�ϸ����ݰ����������ݰ�������״̬λΪESTABLISHED
            LinkNode->nln_5set      = *Cur5Set;
            LinkNode->nln_status    = ESTABLISHED;
            LinkNode->next          = NULL;
            if (strcmp(iptos(sip), "192.168.1.108") == 0) {
                LinkNode->nln_upl_size  = data_len;
                LinkNode->nln_downl_size= 0;
                LinkNode->nln_upl_pkt   = 1;
                LinkNode->nln_downl_pkt = 0;
            } else {
                LinkNode->nln_upl_size  = 0;
                LinkNode->nln_downl_size= data_len;
                LinkNode->nln_upl_pkt   = 0;
                LinkNode->nln_downl_pkt = 1;
            }
            add_to_flowLink(FLowLink_UDP, LinkNode);

            printf("�����(UDP):Դ�˿ں�:%hu -> Ŀ�Ķ˿ں�:%hu, ���ݳ���:%d\n", src_port, dest_port, data_len);
            fprintf(log, "�����(UDP):Դ�˿ں�:%hu -> Ŀ�Ķ˿ں�:%hu, ���ݳ���:%d\n", src_port, dest_port, data_len);
            if (data_len>0 &&( src_port == 53 || dest_port == 53)) {
                dns_header* dns_packet = (dns_header*)((unsigned char *)udp_packet + UDP_LEN);
                u_short tracation_id = dns_packet->dh_transcation_id;
                u_short dh_flags = ntohs(dns_packet->dh_flags);
                int flags_array[16];
                printf("Ӧ�ò�(dns):\n");
                fprintf(log, "Ӧ�ò�(dns):\n");
                display_binary(dh_flags, &flags_array);
                int i=0;
                printf("dns״̬��:");
                fprintf(log, "dns״̬��:");
                for(; i<16; i++) {
                    if (i == 1 || i == 5 || i ==6 || i == 7 || i == 8 || i == 9 || i == 12)
                        printf(" ");
                    printf("%d", flags_array[i]);
                }
                printf("\n");
                fprintf(log, "\n");
                unsigned short questions = ntohs(dns_packet->dh_questions);
                unsigned short answers = ntohs(dns_packet->dh_answers);
                unsigned short authorities = ntohs(dns_packet->dh_authority);
                unsigned short addtionals = ntohs(dns_packet->dh_additional);
                printf("������:%d,�ش���:%d,��Ȩ��:%d,������:%d\n", questions, answers, authorities, addtionals);
                fprintf(log, "������:%d,�ش���:%d,��Ȩ��:%d,������:%d\n", questions, answers, authorities, addtionals);
                if (questions > 0) {
                    dns_querry* dns_querry_text = (dns_querry*)((unsigned char*)dns_packet + DNS_HEAD_LEN);
                    int length = dns_querry_text->length;
                    printf("������ѯ:");
                    fprintf(log, "������ѯ:");
                    while (length > 0) {
                        unsigned char* domain_name = (char *)malloc(sizeof(char)*(length));
                        strncpy(domain_name, (unsigned char*)(dns_querry_text + 1), length);
                        dns_querry_text = (dns_querry*)((unsigned char *)dns_querry_text + 1 + length);
                        length = dns_querry_text->length;
                        printf("%s", domain_name);
                        fprintf(log, "%s", domain_name);
                        if (length != 0) {
                            printf(".");
                        }
                    }
                    dns_type_class* query_type_class = (dns_type_class*)((unsigned char*)(dns_querry_text + 1));
                    u_short type = ntohs(query_type_class->type);
                    u_short query_class = ntohs(query_type_class->query_class);
                    printf(",��ѯ����:%d, ��ѯ��:%d\n", type, query_class);
                    fprintf(log, ",��ѯ����:%d, ��ѯ��:%d\n", type, query_class);
                }
            }
        }
        printf("\n");
        fprintf(log, "\n\n");
    }
}

int main(int argc,char *argv[])
{
	pcap_if_t *alldevs;
	pcap_if_t *d;
	struct in_addr net_ip_address;
	u_int32_t net_ip;
	char *net_ip_string;
	struct bpf_program  fcode;
    pcap_t *dev_handle;
    char *packet_filters[] = {"", "ip", "tcp", "udp", "port 80", "port 53"};
    char *packet_filter;
	struct in_addr net_mask_address;
	u_int32_t net_mask;
	char *net_mask_string;
    //tcp����ͷ�ڵ�
    FLowLink_TCP = (net_link_header *)malloc(sizeof(net_link_header));
    //��ʼ��tcp����
    init_flowLink(FLowLink_TCP);
    //udp����ͷ�ڵ�
    FLowLink_UDP = (net_link_header *)malloc(sizeof(net_link_header));
    //��ʼ��udp����
    init_flowLink(FLowLink_UDP);

	int i=1;
	char errbuf[PCAP_ERRBUF_SIZE];
	if(pcap_findalldevs(&alldevs,errbuf)==-1)//�޷��ҵ������б�
	{
		fprintf(stderr,"error in pcap_findalldevs: %s\n",errbuf);
		exit(1);
	}
	/* ɨ���б� */
	FILE *log = fopen("log.txt", "w");
	FILE *fOutput = fopen("result.data", "w");
	fclose(log);
	fclose(fOutput);
	for(d=alldevs;d;d=d->next)
	{
	    printf("[%d]\n", i++);
		printf("%s\n",d->name);
		printf("Description: %s\n",d->description);
		pcap_lookupnet(d->name,&net_ip,&net_mask,errbuf);

		net_ip_address.s_addr = net_ip;
		net_ip_string = inet_ntoa(net_ip_address);//format
		printf("�����ַ: %s \n",net_ip_string);

		net_mask_address.s_addr = net_mask;
		net_mask_string = inet_ntoa(net_mask_address);//format
		printf("��������: %s \n",net_mask_string);
		printf("\n");
	}
	/* �ͷ����� */
	int num;
	printf("��������Ҫѡ����豸���(1-%d):\n", --i);
	scanf("%d", &num);
	if(num < 1 || num > i){
        printf("���뷶Χ����\n");
        //�ͷ��豸�б�
        pcap_freealldevs(alldevs);
        return -1;
    }
    for(d = alldevs, i=0; i < num-1 ;d = d->next, i++);

    /* ��ñ���IP */
    struct pcap_addr *addr = d->addresses;
    struct sockaddr_in *sin;
    for(;addr;addr = addr->next){
        sin = (struct sockaddr_in *)addr->addr;
        net_ip_string = inet_ntoa(sin->sin_addr);

    }
    printf("��ѡ���������:%s\n��������ip��ַ��:%s\n", d->name, net_ip_string);
    FILE *fp = fopen("E:\\ip.txt", "w");
    fprintf(fp, net_ip_string);
    if ((dev_handle = pcap_open(d->name,
                                65536,    // 65535��֤�ܲ��񵽲�ͬ������·���ϵ�ÿ�����ݰ���ȫ������
                                PCAP_OPENFLAG_PROMISCUOUS,      //�����������ģʽ
                                1000,   // ��ȡ��ʱʱ��
                                NULL,
                                errbuf)) == NULL){
        fprintf(stderr,"\n��������ʧ��,��֧��%s\n", d->name);
        //�ͷ��豸�б�
        pcap_freealldevs(alldevs);
        return -1;
    }
    printf("������˹���:\n[1].Ethernet\n[2].IP\n[3].TCP\n[4].UDP\n[5].HTTP\n[6].DNS\n");
    scanf("%d", &num);
    if(num < 1 || num > 6){
        printf("���뷶Χ����,Ĭ��Ϊ��ѡ��MAC����\n");
        packet_filter = "";
    } else {
        //packet_filter = packet_filters[num-1];
        packet_filter = packet_filters[num-1];
        printf("��ѡ��Ĺ��˹�����:%s\n", packet_filter);
    }
    if (cycle <= 0) {
        printf("������ķ������ڲ�����,���Զ�Ϊ��ѡ��1s��������");
        cycle = 1;
    } else {
        printf("��ѡ��ķ���������%d", cycle);
    }
    pcap_lookupnet(d->name,&net_ip,&net_mask,errbuf);
    if (pcap_compile(dev_handle, &fcode, packet_filter, 1, net_mask) <0 )
    {
        printf("\nUnable to compile the packet filter. Check the syntax.\n");
        return 0;
    }
    //set the filter
    if (pcap_setfilter(dev_handle, &fcode) < 0)
    {
        printf("\nError setting the filter.\n");
        return 0;
    }
    printf("���ڼ���:%s...\n", d->description);
	pcap_freealldevs(alldevs);
	printf("\n");
	pcap_dumper_t *dumpfile;
    dumpfile = pcap_dump_open(dev_handle, "traffic.data");

	pcap_loop(dev_handle, CAPTURE_PACKET_NUM, cb_getPacket, (u_char*)dumpfile);
	pcap_dump_close(dumpfile);
    pcap_close(dev_handle);
    printf("\n��ɨ�������!");
    return 0;
}
