#include "skel.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <queue.h>
#include <stdbool.h>

#define ETH_HEADER sizeof(struct ether_header)
#define ETH_AND_IP_HEADER sizeof(struct ether_header) + sizeof(struct iphdr)

struct route_table_entry {
	uint32_t prefix;
	uint32_t next_hop;
	uint32_t mask;
	int interface;
} __attribute__((packed));

struct arp_entry {
	__u32 ip;
	uint8_t mac[6];
};

struct route_table_entry* rtable;
int rtable_size;
struct arp_entry *arp_table;
int arp_table_size;

//functia de comparatie pentru qsort
int cmpfunc(const void* a, const void *b) {
	if (( (*(struct route_table_entry*)a).prefix - 
		(*(struct route_table_entry*)b).prefix) == 0)
		return ((*(struct route_table_entry*)a).mask - 
			(*(struct route_table_entry*)b).mask);
	return ((*(struct route_table_entry*)a).prefix - 
		(*(struct route_table_entry*)b).prefix);
}

//converteste un uint32_t in __uint8_t[]
void ip_to_arr(uint32_t ip_num, __uint8_t ip[4]) {
	uint32_t mask = 255;

	ip[0] = ip_num & mask;
	ip[1] = (ip_num & (mask << 8)) >> 8;
	ip[2] = (ip_num & (mask << 16)) >> 16;
	ip[3] = (ip_num & (mask << 24)) >> 24;
}

//converteste un __uint8_t[] in uint32_t
uint32_t arr_to_ip(uint8_t ip_arr[4]){

	return ip_arr[0] + (ip_arr[1] << 8) +
		(ip_arr[2] << 16) + (ip_arr[3] << 24);
}

//separa linia dupa spatiu, returneaza un tabel
void tokenizer(char string_arr[4][20], char* line) {
	int arr_size = 0;
	char *t;

	t = strtok(line, " ");

	while (t != NULL) {
		strcpy(string_arr[arr_size], t);
		arr_size = arr_size + 1;
		t = strtok(NULL, " ");
	}
	string_arr[3][1] = 0;
}

//citeste tabela de rutare din fisier
int read_rtable() {
	char *one_line = NULL;
	size_t one_line_size = 0;
	int line_count = 0;
	ssize_t line_size;
	FILE *read_file = fopen("rtable.txt", "r");
	char string_arr[4][20];

	if (!read_file) {
		printf("Can't open the file\n");
		return EXIT_FAILURE;
	}

	//citesc o linie din fisier, pastrez dimensiunea in octeti
	line_size = getline(&one_line, &one_line_size, read_file);

	while (line_size >= 0) {
		line_count++;

		//daca nu mai exista loc in tabela, realoc memorie
		if(line_count >= rtable_size){
			rtable_size *= 2;
			rtable = realloc(rtable, sizeof(struct route_table_entry)*rtable_size);
		}

		tokenizer(string_arr, one_line);
		
		//completez campurile celulei noi din tabela
		rtable[line_count-1].prefix = inet_addr(string_arr[0]);
		rtable[line_count-1].next_hop = inet_addr(string_arr[1]);
		rtable[line_count-1].mask = inet_addr(string_arr[2]);
		rtable[line_count-1].interface = (int)(string_arr[3][0]-'0');
	
		//citesc urmatoare linie
		line_size = getline(&one_line, &one_line_size, read_file);
	}
	fclose(read_file);

	return line_count;
}

//verifica daca elementul exista in tabela arp
bool exist_in_arp(__u32 ip, uint8_t mac[6]) {
	for (int i = 0; i < arp_table_size; i++) 
		if (arp_table[i].ip == ip && 
			memcmp(mac, arp_table[i].mac, 6) == 0)
			return true; 

	return false;
}

//adauga in tabela de arp
void add_to_arp_table(uint32_t ip, uint8_t mac[6]) {
	//daca asa intrare exista, nu se mai adauga
	if (exist_in_arp(ip, mac))
		return;
	//daca tabela arp e goala, se aloca memorie pentru o intrare
	if (arp_table == NULL){
		arp_table_size = 1;
		arp_table = malloc(sizeof(struct arp_entry) * arp_table_size);
	}
	//daca nu e goala, se realoca memorie pentru inca o intrare
	else {
		arp_table_size++;
		arp_table = realloc(arp_table, 
			sizeof(struct route_table_entry)*arp_table_size);
	}

	//se adauga intrarea in ultima celula
	arp_table[arp_table_size - 1].ip = ip;
	memcpy(arp_table[arp_table_size - 1].mac, mac, 6);
}

//cautarea binara a destinatiei in tabela de rutare
int binary_search(int left, int right, uint32_t dest_ip) {
	while(left <= right){

		int middle = left + (right-left) / 2;
		int idx = middle, max_mask = 0;

		if(rtable[middle].prefix == (dest_ip & rtable[middle].mask)) {

			//merg in stanga atata timp cat se satisface conditia
			if (middle > 0 && rtable[middle-1].prefix == 
				(rtable[middle-1].mask & dest_ip)){
					middle--;
					while (middle > 0 && rtable[middle].prefix == 
						(rtable[middle].mask & dest_ip))
							middle--;
					middle++;
			}

			//aleg masca cea mai mare
			while(((rtable[middle].mask & dest_ip) == rtable[middle].prefix) 
				&& (rtable[middle].mask > max_mask)){

				max_mask = rtable[middle].mask;
				idx = middle;
				middle++;
			}
			return idx;
		}
		if(rtable[middle].prefix > (dest_ip & rtable[middle].mask)) 
			return binary_search(left, middle - 1, dest_ip);
		if(rtable[middle].prefix < (dest_ip & rtable[middle].mask)) 
			return binary_search(middle + 1, right, dest_ip);
	}
	return -1;	
}

//cauta cea mai buna ruta
struct route_table_entry *get_best_route(uint32_t dest_ip) {
	int index = binary_search(0, rtable_size-1, dest_ip);

	if(index != -1)
		return &rtable[index];

	return NULL;
}


//cauta intrarea corespunzatoare ip-ului in tabela arp
struct arp_entry *get_arp_entry(uint32_t ip) {
	for(int i =0 ; i < arp_table_size; i++) 
		if (arp_table[i].ip == ip)
			return &arp_table[i];
	return NULL;
}

//initializeaza si returneaza pachetul icmp reply
packet init_icmp_packet(packet received, uint8_t icmp_type) {
	
	//creez pachetul
	packet pkt;
	memset(pkt.payload, 0, sizeof(pkt.payload));

	//extrag header-ele pachetului nou
	struct ether_header *pkt_eth_hdr = (struct ether_header *)pkt.payload;
	struct iphdr *pkt_ip_hdr = (struct iphdr *)(pkt.payload + ETH_HEADER);
	struct icmphdr *pkt_icmp_hdr = (struct icmphdr *)(pkt.payload + ETH_AND_IP_HEADER);
	struct ether_header *received_eth_hdr = (struct ether_header *)received.payload;
	struct iphdr *received_ip_hdr = (struct iphdr *)(received.payload + ETH_HEADER);

	pkt.len = ETH_AND_IP_HEADER + sizeof(struct icmphdr);

	//completez header-ul de ethernet
	memcpy(pkt_eth_hdr->ether_dhost, received_eth_hdr->ether_shost, 6);
	memcpy(pkt_eth_hdr->ether_shost, received_eth_hdr->ether_dhost, 6);
	pkt_eth_hdr->ether_type = htons(ETHERTYPE_IP);

	//completez header-ul de ip
	pkt_ip_hdr->version = 4;
	pkt_ip_hdr->ihl = 5;
	pkt_ip_hdr->tos = 0;
	pkt_ip_hdr->tot_len = htons(pkt.len - ETH_HEADER);
	pkt_ip_hdr->id = 0;
	pkt_ip_hdr->frag_off = 0;
	pkt_ip_hdr->ttl = 64;
	pkt_ip_hdr->protocol = IPPROTO_ICMP;
	pkt_ip_hdr->saddr = received_ip_hdr->saddr;
	pkt_ip_hdr->daddr = received_ip_hdr->daddr;
	pkt_ip_hdr->check = 0;
	pkt_ip_hdr->check = ip_checksum(pkt_ip_hdr, sizeof(struct iphdr));
	
	//completez header-ul de icmp
	pkt_icmp_hdr->code = 0;
	pkt_icmp_hdr->type = icmp_type;
	pkt_icmp_hdr->un.echo.id = 0;
	pkt_icmp_hdr->un.echo.sequence = 0;
	pkt_icmp_hdr->checksum = 0;
	pkt_icmp_hdr->checksum = ip_checksum(pkt_icmp_hdr, sizeof(struct icmphdr));

	return pkt;
}

//initializeaza pachetul arp request sau reply
packet init_arp_packet(short p_type, packet m, uint32_t dest_ip) {
	packet pkt;
	memset(pkt.payload, 0, sizeof(pkt.payload));
	
	//extrage header-ele necesare a pachetului nou si a celui primit
	struct ether_header *pkt_eth_hdr = (struct ether_header*)pkt.payload;
	struct ether_arp *pkt_eth_arp = (struct ether_arp *)(pkt.payload + ETH_HEADER);
	struct ether_header *m_eth_hdr = (struct ether_header *)m.payload;
	struct ether_arp *m_eth_arp = (struct ether_arp *)(m.payload + ETH_HEADER);

	pkt.len = ETH_HEADER + sizeof(struct ether_arp);
	
	//completez header-ul de ethernet
	pkt_eth_hdr->ether_type = htons(ETHERTYPE_ARP);

	//verific daca e request sau reply
	if (p_type == ARPOP_REQUEST)
		for (int i = 0; i < 6; ++i)
			pkt_eth_hdr->ether_dhost[i] = 255;
	else
		memcpy(pkt_eth_hdr->ether_dhost, m_eth_hdr->ether_shost, 6);

	get_interface_mac(m.interface, pkt_eth_hdr->ether_shost);


	//completez header-ul arp
	pkt_eth_arp->ea_hdr.ar_hrd = htons(1);
	pkt_eth_arp->ea_hdr.ar_pro = htons(0x0800);
	pkt_eth_arp->ea_hdr.ar_hln = 6;
	pkt_eth_arp->ea_hdr.ar_pln = 4;
	pkt_eth_arp->ea_hdr.ar_op = htons(p_type); 

	//request -> setes adresele sursa, pe baza interfetei din pachet
	//completez tpa cu destinatia din pachetul primit
	if (p_type == ARPOP_REQUEST) {
		get_interface_mac(m.interface, pkt_eth_arp->arp_sha);
		ip_to_arr(inet_addr(get_interface_ip(m.interface)), pkt_eth_arp->arp_spa);
		ip_to_arr(dest_ip, pkt_eth_arp->arp_tpa);
	}
	
	//reply -> interschimb adresele sha, spa, si tpa
	//la adresa sha pun ip-ul interfetei mele
	else {
		memcpy(pkt_eth_arp->arp_tha, m_eth_arp->arp_sha, 6);
		get_interface_mac(m.interface, pkt_eth_arp->arp_sha);
		memcpy(pkt_eth_arp->arp_tpa, m_eth_arp->arp_spa, 4);
		memcpy(pkt_eth_arp->arp_spa, m_eth_arp->arp_tpa, 4);
	}

	return pkt;
}

int main(int argc, char *argv[]) {
	packet m;
	int rc;
	init();

	//initializez dimensiunea tabelei de rutare si aloc memorie tabelei
	rtable_size = 50;
	rtable = malloc(sizeof(struct route_table_entry) * rtable_size);

	//apelez functia pentru citirea si completarea tabelei de rutare
	rtable_size = read_rtable();

	//sortez tabela de rutare
	qsort(rtable, rtable_size, sizeof(struct route_table_entry), cmpfunc);

	//creez coada de pachete in asteptare
	struct queue *packet_queue = queue_create();

	while (1) {
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");

		//extrag pachetul de ethernet
		struct ether_header *eth_hdr = (struct ether_header *)m.payload;

		//verific daca e un pachet IP
		if (eth_hdr->ether_type == htons(ETHERTYPE_IP)) {

			struct iphdr *ip_hdr = (struct iphdr *)(m.payload + ETH_HEADER);
			
			//verific checksum-ul
			if (ip_checksum(ip_hdr, sizeof(struct iphdr)) != 0)
				continue;

			//verific daca pachetul nu a expirat
			if (ip_hdr->ttl <= 1) {

				//creez ...
				packet reply_packet = init_icmp_packet(m, ICMP_TIME_EXCEEDED);

				//...si trimit un pachet icmp inapoi de tip ICMP_TIME_EXCEEDED
				send_packet(m.interface, &reply_packet);

				continue;
			}

			struct icmphdr *icmp_hdr = (struct icmphdr *)(m.payload + ETH_AND_IP_HEADER);
			
			//verific daca pachetul e de tip ecou si e destinat mie
			if(icmp_hdr->type == ICMP_ECHO &&
				inet_addr(get_interface_ip(m.interface)) == ip_hdr->daddr) {

				//creez ...
				packet reply_packet = init_icmp_packet(m, ICMP_ECHOREPLY);

				//...si trimit un pachet ICMP_ECHOREPLY inapoi la expeditor
				send_packet(m.interface, &reply_packet);

				continue;
			}
			
			//scad ttl-ul, recalculez checksum-ul
			ip_hdr->ttl--;
			ip_hdr->check = 0;
			ip_hdr->check = ip_checksum(ip_hdr, sizeof(struct iphdr));

			struct route_table_entry *best_route = get_best_route(ip_hdr->daddr);

			//verific daca exista ruta pentru destinatia din pachet
			if (best_route == NULL){

				//creez...
				packet reply_packet = init_icmp_packet(m, ICMP_DEST_UNREACH);

				//... si trimit un pachet icmp inapoi de tip ICMP_DEST_UNREACH
				send_packet(m.interface, &reply_packet);

				continue;
			}

			struct arp_entry *arp_entry = get_arp_entry(ip_hdr->daddr);

			//verific daca exista intrare in tabela arp dupa ip-ul din pachet
			if (arp_entry == NULL){

				//pastrez interfata din best route in pachet
				m.interface = best_route->interface;

				//creez o copie a pachetului primit
				packet *copy = malloc(sizeof(packet));

				copy->interface = m.interface;
				copy->len = m.len;
				memcpy(copy->payload, m.payload, m.len);

				//il adaug in coada
				queue_enq(packet_queue, copy);

				//creez un pachet arp request pentru a primi mac-ul...
				packet arp_request = init_arp_packet(ARPOP_REQUEST, m, best_route->next_hop);

				//...si il trimit
				send_packet(best_route->interface, &arp_request);

				continue;
			}

			//setez adresa mac sursa si destinatie
			get_interface_mac(best_route->interface, eth_hdr->ether_shost);
			memcpy(eth_hdr->ether_dhost, arp_entry->mac, 6);

			//trimit pachetul mai departe
			send_packet(best_route->interface, &m);
		}
		
		//verific daca e un pachet ARP
		if (eth_hdr->ether_type == htons(ETHERTYPE_ARP)) {
			//extrag header-ul de ARP
			struct ether_arp *eth_arp = (struct ether_arp *)(m.payload + ETH_HEADER);

			uint32_t ip = arr_to_ip(eth_arp->arp_tpa);

			//verific daca e request si e pentru mine
			if (eth_arp->ea_hdr.ar_op == htons(ARPOP_REQUEST) && 
				ip == inet_addr(get_interface_ip(m.interface))) {

					//creez pachetul reply, setand adresa mac a interfetei mele
					packet arp_packet = init_arp_packet(ARPOP_REPLY, m, 
						inet_addr(get_interface_ip(m.interface)));

					send_packet(m.interface, &arp_packet);
					continue;
			}
			//verific daca e reply
			if (eth_arp->ea_hdr.ar_op == htons(ARPOP_REPLY)) { 
				uint32_t ip = arr_to_ip(eth_arp->arp_spa);
				
				//adaug intrarea in tabela ARP
				add_to_arp_table(ip, eth_arp->arp_sha);

				//trimit pachete pana se elibereaza coada
				while (!queue_empty(packet_queue)) {

					//extrag cate un pachet din coada
					packet *p = (packet *)queue_deq(packet_queue);
					struct ether_header *p_eth_hdr = (struct ether_header *)p->payload;

					//setez adresele mac a pachetului
					get_interface_mac(p->interface, p_eth_hdr->ether_shost);
					memcpy(p_eth_hdr->ether_dhost, eth_arp->arp_sha, 6);

					//trimit pachetul din coada de asteptare
					send_packet(p->interface, p);

				}
				continue;
			}
		}	
	}
	return 0;
}
