#include "pico_config.h"
#include "pico_eth.h"
#include "pico_socket.h"
#include "pico_stack.h"
#include "pico_socket.h"
#include "pico_queue.h"
#include "pico_tree.h"
#include "modules/pico_igmp.c"
#include "check.h"
#include "pico_dev_null.c"
Suite *pico_suite(void);
struct pico_timer *pico_timer_add(pico_time expire, void (*timer)(pico_time, void *), void *arg) 
{
    IGNORE_PARAMETER(expire);
    IGNORE_PARAMETER(timer);
    IGNORE_PARAMETER(arg);
    return NULL;
}
static int mcast_filter_cmp(void *ka, void *kb)
{
    union pico_address *a = ka, *b = kb;
    if (a->ip4.addr < b->ip4.addr)
        return -1;

    if (a->ip4.addr > b->ip4.addr)
        return 1;

    return 0;
}
static int mcast_sources_cmp(void *ka, void *kb)
{
    union pico_address *a = ka, *b = kb;
    if (a->ip4.addr < b->ip4.addr)
        return -1;

    if (a->ip4.addr > b->ip4.addr)
        return 1;

    return 0;
}
PICO_TREE_DECLARE(_MCASTFilter, mcast_filter_cmp);
START_TEST(tc_pico_igmp_report_expired)
{
    struct igmp_timer t;
    struct pico_ip4 zero = {{0}};
    t.mcast_link = zero;
    t.mcast_group = zero;
    //void function, just check for side effects
    pico_igmp_report_expired(&t);
}
END_TEST
START_TEST(tc_igmpt_type_compare) 
{
    struct igmp_timer a;
    struct igmp_timer b;
    a.type = 1;
    b.type = 2;
    fail_if(igmpt_type_compare(&a,&b) != -1);
    fail_if(igmpt_type_compare(&b,&a) != 1);
    fail_if(igmp_timer_cmp(&b,&a) != 1);
}
END_TEST
START_TEST(tc_pico_igmp_state_change) {
    struct pico_ip4 mcast_link, mcast_group;
    struct igmp_parameters p;
    pico_string_to_ipv4("192.168.1.1", &mcast_link.addr);
    pico_string_to_ipv4("224.7.7.7", &mcast_group.addr);
    p.mcast_link = mcast_link;
    p.mcast_group = mcast_group;
    fail_if(pico_igmp_state_change(&mcast_link, &mcast_group, 0,NULL, 99) != -1);
    fail_if(pico_igmp_state_change(&mcast_link, &mcast_group, 0,NULL, PICO_IGMP_STATE_CREATE) != 0);
}
END_TEST
START_TEST(tc_pico_igmp_process_in) {
    struct igmp_parameters *p;
    struct pico_device *dev = pico_null_create("dummy3");
    struct pico_ipv4_link *link;
    int i,j, _i,_j,result;
    struct pico_mcast_group g; 
    //Building example frame
    p = PICO_ZALLOC(sizeof(struct igmp_parameters));
    pico_string_to_ipv4("192.168.1.1", &p->mcast_link.addr);
    pico_string_to_ipv4("244.7.7.7", &p->mcast_group.addr);
    pico_ipv4_link_add(dev, p->mcast_link, p->mcast_link);
    link = pico_ipv4_link_get(&p->mcast_link);
    link->mcast_compatibility = PICO_IGMPV2;
    g.mcast_addr = p->mcast_group;
    g.MCASTSources.root = &LEAF;
    g.MCASTSources.compare = mcast_sources_cmp;
    pico_tree_insert(link->MCASTGroups, &g);
    pico_tree_insert(&IGMPParameters, p);
    
    fail_if(pico_igmp_generate_report(p) != 0);
    fail_if(pico_igmp_process_in(NULL,p->f) != 0);
    
    link->mcast_compatibility = PICO_IGMPV3;
    for(_j =0; _j<3; _j++) {   //FILTER
        (_j == 2) ? (result = -1) : (result = 0);
        for(_i=0; _i<3; _i++) {  //FILTER
            if(_i == 2) result = -1;
            for(i = 0; i<3; i++) {  //STATES
                for(j = 0; j<6; j++) { //EVENTS
                    p->MCASTFilter = &_MCASTFilter;
                    p->filter_mode = _i;
                    g.filter_mode = _j;
                    if(p->event == IGMP_EVENT_DELETE_GROUP || p->event == IGMP_EVENT_QUERY_RECV)
                        p->event++;
                    fail_if(pico_igmp_generate_report(p) != result);
                    p->state = i;
                    p->event = j;
                    if(result != -1 && p->f)//in some combinations, no frame is created
                        fail_if(pico_igmp_process_in(NULL,p->f) != 0);
                }
            }
        }
    }
}
END_TEST
START_TEST(tc_pico_igmp_compatibility_mode) {
    struct pico_frame *f;
    struct pico_device *dev = pico_null_create("dummy1");
    struct pico_ip4 addr;
    struct pico_ipv4_hdr *hdr;
    struct igmp_message *query;
    uint8_t ihl =24;
    f = pico_proto_ipv4.alloc(&pico_proto_ipv4, sizeof(struct igmpv3_report)+sizeof(struct igmpv3_group_record) +(0 *sizeof(struct pico_ip4)));
    pico_string_to_ipv4("192.168.1.1", &addr.addr);
    hdr = (struct pico_ipv4_hdr *) f->net_hdr;
    ihl = (uint8_t)((hdr->vhl & 0x0F) * 4); /* IHL is in 32bit words */
    query = (struct igmp_message *) f->transport_hdr;
    //No link
    fail_if(pico_igmp_compatibility_mode(f) != -1); 
    pico_ipv4_link_add(dev, addr, addr);
    f->dev = dev;
    //Igmpv3 query
    hdr->len = short_be(12 + ihl);
    fail_if(pico_igmp_compatibility_mode(f) != 0); 
    //Igmpv2 query
    hdr->len = short_be(8 + ihl);
    query->max_resp_time =0;
    fail_if(pico_igmp_compatibility_mode(f) == 0);
    query->max_resp_time =1;
    fail_if(pico_igmp_compatibility_mode(f) != 0);
    //Invalid Query 
    hdr->len = short_be(9 + ihl);
    fail_if(pico_igmp_compatibility_mode(f) == 0);
}
END_TEST
START_TEST(tc_pico_igmp_analyse_packet) {
    struct pico_frame *f;
    struct pico_device *dev = pico_null_create("dummy0");
    struct pico_ip4 addr;
    struct pico_ipv4_hdr *ip4;
    struct igmp_message *igmp;
    f = pico_proto_ipv4.alloc(&pico_proto_ipv4, sizeof(struct igmp_message));
    pico_string_to_ipv4("192.168.1.1", &addr.addr);
    //No link
    fail_if(pico_igmp_analyse_packet(f) != NULL); 
    pico_ipv4_link_add(dev, addr, addr);
    f->dev = dev;
    ip4 = f->net_hdr;
    
    igmp = (struct igmp_message *) (f->transport_hdr);
    igmp->type = 0;
    //wrong type
    fail_if(pico_igmp_analyse_packet(f) != NULL);

    // all correct
    igmp->type = IGMP_TYPE_MEM_QUERY;
    fail_if(pico_igmp_analyse_packet(f) == NULL);
    igmp->type = IGMP_TYPE_MEM_REPORT_V1;
    fail_if(pico_igmp_analyse_packet(f) == NULL);
    igmp->type = IGMP_TYPE_MEM_REPORT_V2;
    fail_if(pico_igmp_analyse_packet(f) == NULL);
    igmp->type = IGMP_TYPE_MEM_REPORT_V3;
    fail_if(pico_igmp_analyse_packet(f) == NULL);
}
END_TEST
START_TEST(tc_pico_igmp_discard) {
    /* TODO */
}
END_TEST
Suite *pico_suite(void)
{

    Suite *s = suite_create("PicoTCP");

    TCase *TCase_pico_igmp_report_expired = tcase_create("Unit test for pico_igmp_report_expired");
    TCase *TCase_igmpt_type_compare = tcase_create("Unit test for igmpt_type_compare");
    TCase *TCase_pico_igmp_analyse_packet = tcase_create("Unit test for pico_igmp_analyse_packet");
    TCase *TCase_pico_igmp_discard = tcase_create("Unit test for pico_igmp_discard");
    TCase *TCase_pico_igmp_compatibility_mode = tcase_create("Unit test for pico_igmp_compatibility");
    TCase *TCase_pico_igmp_state_change = tcase_create("Unit test for pico_igmp_state_change");
    TCase *TCase_pico_igmp_process_in = tcase_create("Unit test for pico_igmp_process_in");
    
    tcase_add_test(TCase_pico_igmp_report_expired, tc_pico_igmp_report_expired);
    suite_add_tcase(s, TCase_pico_igmp_report_expired);
    tcase_add_test(TCase_igmpt_type_compare, tc_igmpt_type_compare);
    suite_add_tcase(s, TCase_igmpt_type_compare);
    tcase_add_test(TCase_pico_igmp_analyse_packet, tc_pico_igmp_analyse_packet);
    suite_add_tcase(s, TCase_pico_igmp_analyse_packet);
    tcase_add_test(TCase_pico_igmp_discard, tc_pico_igmp_discard);
    suite_add_tcase(s, TCase_pico_igmp_discard);
    tcase_add_test(TCase_pico_igmp_compatibility_mode, tc_pico_igmp_compatibility_mode);
    suite_add_tcase(s, TCase_pico_igmp_compatibility_mode);
    suite_add_tcase(s, TCase_pico_igmp_state_change);
    tcase_add_test(TCase_pico_igmp_state_change, tc_pico_igmp_state_change);
    suite_add_tcase(s, TCase_pico_igmp_process_in);
    tcase_add_test(TCase_pico_igmp_process_in, tc_pico_igmp_process_in);
    return s;
}

int main(void)
{
    int fails;
    Suite *s = pico_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    fails = srunner_ntests_failed(sr);
    srunner_free(sr);
    return fails;
}
