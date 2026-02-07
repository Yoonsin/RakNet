#include <uapi/linux/ptrace.h>
#include <net/sock.h>
#include <bcc/proto.h>
#include <linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/in6.h>
#include <uapi/linux/ipv6.h>
#include <uapi/linux/pkt_cls.h>
#include <uapi/linux/bpf.h>
#include "net_monitor.h"

#define IP_TCP 6
#define IP_UDP 17
#define IP_ICMP 1
#define ETH_HLEN 14
#define DEFAULT_CLASS_ID 0x10030//0x10030

struct data_t {
    u64 ts;
    char msg[32];
    u32 len;
};

// Map key struct for IP traffic
struct statkey {
    u32 srcip;    // source IPv6 address
    u32 dstip;    // destination IPv6 address
};

// Map value struct with counters
struct statvalue {
    u64 packets;    // packets ingress + egress
    u64 bytes;      // bytes ingress + egress
	u64 bytes_prev_1s;     
    u64 duration;
	u64 ts_init;    // first seen timestamp
    u64 ts_last;    // last seen timestamp
	u64 ts_prev_1s; // timestamp 1 second ago
	u64 avgBytes;   // average bytes per packet
	u64 throughput_instant; // instantaneous throughput (1s interval)
    struct bpf_spin_lock lock;
};

BPF_PERF_OUTPUT(events);
BPF_HASH(packet_cnt, struct statkey, struct statvalue);
BPF_HASH(ip_to_class_map, u32, u32);

static inline __attribute__((always_inline)) void
print_app(struct __sk_buff* skb, char* str)
{
    struct data_t evt = {};
    evt.ts = bpf_ktime_get_ns();
    strncpy(evt.msg, str, sizeof(evt.msg) - 1);
    events.perf_submit(skb, &evt, sizeof(evt));
}

//TODO : each packet handler create (ref : https://github.com/iovisor/bcc/blob/master/examples/networking/tunnel_monitor/monitor.py)
int handle_ingress(struct __sk_buff* skb) {

    u8* cursor = 0;
    u32 saddr, daddr;
    long* count = 0;
    long one = 1;

    struct ethernet_t* ethernet = cursor_advance(cursor, sizeof(*ethernet));
    struct ip_t* ip = cursor_advance(cursor, sizeof(*ip));

    if (ip->ver != 4)
        return 1;
    if (ip->nextp != IP_UDP)
        return 1;

    //ONLY IPv4 + UDP
    saddr = ip->src;
    daddr = ip->dst;

    struct statkey key = { ip->src, ip->dst };
    struct statvalue* val = packet_cnt.lookup(&key);

    /*  struct data_t evt = {};
      evt.len = skb->len;
      events.perf_submit(skb, &evt, sizeof(evt));*/

	//TODO : CPU Preemption issue Check (using skb->cb)
    if (val) {
        u64 cur_time = bpf_ktime_get_ns();
        bpf_spin_lock(&val->lock);

        val->packets++;
		val->bytes += skb->len;
        val->duration = cur_time - val->ts_last;
        if (skb->len >= val->avgBytes) val->avgBytes += (skb->len - val->avgBytes) / val->packets;
        else val->avgBytes -= (val->avgBytes - skb->len) / val->packets;

		// instant-throughput (1s interval) calculation
		if (cur_time - val->ts_prev_1s >= 1000000000) { 
			u64 delta_bytes = val->bytes - val->bytes_prev_1s;
			u64 delta_time = cur_time - val->ts_prev_1s;
			val->throughput_instant = delta_bytes * 1000000000 / delta_time;

            val->ts_prev_1s = cur_time;
            val->bytes_prev_1s = val->bytes;
        }
            
        val->ts_last = cur_time;
		bpf_spin_unlock(&val->lock);
    }
    else {
        struct statvalue initval = { .packets = 1, .bytes = skb->len, .ts_init = bpf_ktime_get_ns(), .ts_last = bpf_ktime_get_ns(), .duration = 0 , .avgBytes = skb->len, .ts_prev_1s = bpf_ktime_get_ns(), .bytes_prev_1s = skb->len, .throughput_instant = 0 };
        packet_cnt.update(&key, &initval);
    }

    return 1;

}

int handle_egress(struct __sk_buff* skb) {
    
    u8* cursor = 0;
    u32 saddr, daddr;
    long* count = 0;
    long one = 1;

    struct ethernet_t* ethernet = cursor_advance(cursor, sizeof(*ethernet));
    struct ip_t* ip = cursor_advance(cursor, sizeof(*ip));

    u32 dst_ip = ip->dst;
    u32* class_id = ip_to_class_map.lookup(&dst_ip);
    u32 class_id_value = DEFAULT_CLASS_ID;

    if (class_id)
        class_id_value = TC_H_MAKE(1 << 16, (*class_id) & 0xFFFF);

    if (ip->ver != 4 || ip->nextp != IP_UDP)
        return class_id_value;

	//ONLY IPv4 + UDP stats
    saddr = ip->src;
    daddr = ip->dst;

    struct statkey key = { ip->src, ip->dst };
    struct statvalue* val = packet_cnt.lookup(&key);

     /* struct data_t evt = {};
      evt.len = skb->tc_classid;
      events.perf_submit(skb, &evt, sizeof(evt));*/

      //TODO : CPU Preemption issue Check (using skb->cb)
    if (val) {
        u64 cur_time = bpf_ktime_get_ns();
        bpf_spin_lock(&val->lock);

        val->packets++;
        val->bytes += skb->len;
        val->duration = cur_time - val->ts_last;
        if (skb->len >= val->avgBytes) val->avgBytes += (skb->len - val->avgBytes) / val->packets;
        else val->avgBytes -= (val->avgBytes - skb->len) / val->packets;

        if (cur_time - val->ts_prev_1s >= 1000000000) { // 1 second
            u64 delta_bytes = val->bytes - val->bytes_prev_1s;
            u64 delta_time = cur_time - val->ts_prev_1s;
            val->throughput_instant = delta_bytes * 1000000000 / delta_time;

            val->ts_prev_1s = cur_time;
            val->bytes_prev_1s = val->bytes;
        }

        val->ts_last = cur_time;
        bpf_spin_unlock(&val->lock);
    }
    else {
        struct statvalue initval = { .packets = 1, .bytes = skb->len, .ts_init = bpf_ktime_get_ns(), .ts_last = bpf_ktime_get_ns(), .duration = 0 , .avgBytes = skb->len, .ts_prev_1s = bpf_ktime_get_ns(), .bytes_prev_1s = skb->len, .throughput_instant = 0 };
        packet_cnt.update(&key, &initval);
    }

    return class_id_value;
}
