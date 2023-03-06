#include "emp-tool/utils/block.h"
#include <cstddef>
#include <cstdio>
#include <emp-tool/emp-tool.h>
#include <emp-ot/emp-ot.h>
#include <vector>
#include "./ggm_prf_tree.hpp"
struct One_Hot_Garble{
    std::vector<size_t> input_a;
    //even first odd secend
    std::vector<size_t> level_text;
    std::vector<size_t> output_text;
    std::vector<size_t> output;
    // GT: to_evaluator
    // label: label for clear text value zero
    void garble(block* label, block* to_evaluator,const SequentialC2PC_SH& hash_provider){
        GGM_Hash_Tree_t hash_tree_for_0;
        compute_ggm_prf(label[input_a[0]], hash_tree_for_0, input_a.size(), hash_provider);
        puts("Garbler zero tree 0\n");
        print_tree(hash_tree_for_0);
        GGM_Hash_Tree_t hash_tree_for_1;
        block root_1=xorBlocks(*(label+input_a[0]), hash_provider.Delta);
        compute_ggm_prf(root_1, hash_tree_for_1, input_a.size(), hash_provider);
        puts("Garbler zero tree 1\n");
        print_tree(hash_tree_for_1);
        
        //|Output level cipher pairs|=|input_a|-1
        for (int level_idx=1; level_idx<input_a.size(); level_idx++) {
            block odd_temp=label[input_a[level_idx]];
            print128_num("odd init", odd_temp);
            block even_temp=xorBlocks(odd_temp, hash_provider.Delta);
            print128_num("even init", even_temp);
            for (int leaves_idx=0; leaves_idx<hash_tree_for_0[level_idx].size(); leaves_idx+=2) {
                printf("level %d, even xor with %d leave\n",level_idx,leaves_idx);
                even_temp=xorBlocks(even_temp, hash_tree_for_0[level_idx][leaves_idx]);
                even_temp=xorBlocks(even_temp, hash_tree_for_1[level_idx][leaves_idx]);
                print128_num("even temp", even_temp);
            }
            for (int leaves_idx=1; leaves_idx<hash_tree_for_0[level_idx].size(); leaves_idx+=2) {
                printf("level %d, odd xor with %d leave\n",level_idx,leaves_idx);
                odd_temp=xorBlocks(odd_temp, hash_tree_for_0[level_idx][leaves_idx]);
                odd_temp=xorBlocks(odd_temp, hash_tree_for_1[level_idx][leaves_idx]);
                print128_num("odd temp", odd_temp);
            }
            printf("level %d, odd write to %lu even write to %lu\n",level_idx,level_text[level_idx]*2+1,level_text[level_idx]*2);
            to_evaluator[level_text[level_idx]*2]=even_temp;
            to_evaluator[level_text[level_idx]*2+1]=odd_temp;
        }
        block all_leaves_xor=hash_provider.Delta;
        int idx=0;
        for (auto& a : hash_tree_for_1.back()) {
            all_leaves_xor=xorBlocks(all_leaves_xor, a);
            label[output[idx]]=a;
            idx++;
        }
        for (auto& a : hash_tree_for_0.back()) {
            all_leaves_xor=xorBlocks(all_leaves_xor, a);
            label[output[idx]]=a;
            idx++;
        }
        to_evaluator[output_text[0]]=all_leaves_xor;
    }
    /*
    * label : evaluated wire labels
    */
    void eval(block* label, std::vector<bool> clear_text_a ,block* from_garbler,const SequentialC2PC_SH& hash_provider){
        GGM_Hash_Tree_t hash_tree_a;
        GGM_Hash_Tree_t hash_tree_a_bar;
        compute_ggm_prf(label[input_a[0]], hash_tree_a, input_a.size(), hash_provider);
        puts("Evaluator a tree");
        print_tree(hash_tree_a);
        hash_tree_a_bar.resize(input_a.size());
        int missing_idx=0;
        for(int level_idx=1;level_idx<input_a.size();level_idx++){
            hash_tree_a_bar[level_idx].resize(1<<level_idx);
            //Fill entries on hash_tree_a_bar whose parent is not missing
            for (int leaves_idx=0; leaves_idx<hash_tree_a_bar[level_idx].size();leaves_idx++ ) {
                if(leaves_idx>>1==missing_idx){
                    hash_tree_a_bar[level_idx][leaves_idx]=zero_block();
                }else {
                    printf("compute a bar level %d, idx %d from prev_level\n",level_idx,leaves_idx);
                    hash_provider.Hash(hash_tree_a_bar[level_idx][leaves_idx], hash_tree_a_bar[level_idx-1][leaves_idx/2],leaves_idx&1);
                }
            }
            //Fill entires that is not on the path from the 1 leaf to root on this level
            printf("level %d access gabler in %zu\n",level_idx,level_text[level_idx]*2+!clear_text_a[level_idx]);
            auto garbler_text=xorBlocks(from_garbler[level_text[level_idx]*2+!clear_text_a[level_idx]],label[input_a[level_idx]]);
            print128_num("from garbler", garbler_text);
            for(int leaves_idx=!clear_text_a[level_idx];leaves_idx<hash_tree_a_bar[level_idx].size();leaves_idx+=2){
                printf("xoring with %d\n",leaves_idx);
                garbler_text=xorBlocks(hash_tree_a_bar[level_idx][leaves_idx],garbler_text);
                garbler_text=xorBlocks(hash_tree_a[level_idx][leaves_idx],garbler_text);
                print128_num("eval garbler temp", garbler_text);
            }
            hash_tree_a_bar[level_idx][missing_idx+!clear_text_a[level_idx]]=garbler_text;
            printf("fill a bar level %d, idx %d from generator text\n",level_idx,missing_idx+!clear_text_a[level_idx]);
            missing_idx+=clear_text_a[level_idx];
            missing_idx*=2;
        }
        block all_leaves_xor=from_garbler[output_text[0]];
        for (auto& a : hash_tree_a.back()) {
            all_leaves_xor=xorBlocks(all_leaves_xor, a);
        }
        for (auto& a : hash_tree_a_bar.back()) {
            all_leaves_xor=xorBlocks(all_leaves_xor, a);
        }
        missing_idx/=2;
        
        hash_tree_a_bar.back()[missing_idx]=all_leaves_xor;
        puts("Evaluator a bar tree");
        print_tree(hash_tree_a_bar);
        
        const auto& first_tree=clear_text_a[0]?hash_tree_a:hash_tree_a_bar;
        const auto& second_tree=clear_text_a[0]?hash_tree_a_bar:hash_tree_a;
        int idx=0;
        for (auto& a : first_tree.back()) {
            label[output[idx]]=a;
            idx++;
        }
        for (auto& a : second_tree.back()) {
            label[output[idx]]=a;
            idx++;
        }
    }
};
