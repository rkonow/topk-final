#include "Topk.h"

Topk::Topk(char * file,size_t *file_sizes, int num_files) {
    // Loading the Text
    char *text=NULL;
    // cout << "file = " << file << endl;
    if(loadText(file, &text, &this->length))
        return;
 //   cout << "constructing suffix tree";
 //   cout << "length =" << this->length << endl;
    this->cst = new SuffixTreeY(text, this->length, NAIVE, CN_NPR, 32);
//    cout << "Done!" << endl;
//    cout << "Constructing Suffix Array";
    this->ticsa = new TextIndexCSA((uchar *)text, (ulong)this->length, NULL);
//    cout << "Done!" << endl;
    // removing the text
    delete[] text;

    // Suffix Tree Handler provides several functions
    this->stp = new SuffixTreeHandler(cst,length);
   
    // Retrieve all Nodes in pre-Order
    // preorder_vector[i] -> [l,r]
    vector<pair<uint,uint> >  preorder_vector;
    pair<BitString *,uint> tree_parenthesis = this->stp->generateBitmap(10*length,preorder_vector);
    map<pair<uint,uint>,uint>  nodes;

    // create an inverse map, to index according to [l,r]
    // [x,y] -> i
    for (int i = 0 ; i < preorder_vector.size();i++) {
        nodes[make_pair(preorder_vector[i].first,preorder_vector[i].second)] = i;
    }
    this->nodes_preorder = nodes;
    this->number_of_nodes = preorder_vector.size();
    this->preorder_vector = preorder_vector;

    this->t = new tree_ff(tree_parenthesis.first->getData(),tree_parenthesis.second,OPT_FAST_LCA | OPT_FAST_PREORDER_SELECT | OPT_FAST_LEAF_SELECT);
    delete tree_parenthesis.first;

    // create bitmap Document Array
    BitSequenceRG *bsrg;
    if (make_random_flag)
        bsrg = buildBs(file_sizes,length,rand_files,true);
    else
        bsrg = buildBs(file_sizes,length,num_files);
    this->d = bsrg;

    this->da = new DocumentArray(cst,bsrg,preorder_vector,ticsa); 
    this->ll = new LinkList(2*length);
    this->fillLinkList(num_files);
    this->generateSequence();
}

void Topk::fillLinkList(uint num_files) {
    uint count = 0;
    for (uint i = 1 ; i <= num_files;i++) {
        size_t pos = 0;
        // Retrieve the positions of document i-1 in position pos+1 and pos+2.
        pair<size_t, size_t> pairc = da->selectDocument(i-1,pos+1);
        count++;
       // cout << "node = " << pairc.first << "," << pairc.second << ", count = " << count << endl;
        while(pairc.first <= this->length+1 && pairc.second <= this->length+1) {
            size_t vl_lca,vr_lca;
            // obtain the LCA of both of them
            uint node1 = this->t->Preorden_Select(nodes_preorder[make_pair(pairc.first,pairc.first)]+1);
            uint node2 = this->t->Preorden_Select(nodes_preorder[make_pair(pairc.second,pairc.second)]+1);
            this->cst->LCA(pairc.first,pairc.first,pairc.second,pairc.second,&vl_lca,&vr_lca);
            uint lca = this->t->Lca(node1,node2);
            // if something goes out of place...
            if (lca == (uint)-1)
                break;
            // if (vl_lca == 0 && vr_lca == 0)
            //     break;
            // cout << "node = " << pairc.first << "," << pairc.second << " lca = " << vl_lca << "," << vr_lca << " node = " << this->nodes_preorder[make_pair(vl_lca,vr_lca)] << ", count = " << count << ", freq = " << da->countRange(i-1,vl_lca,vr_lca) <<  endl;
            // insert in the linked list:
            // the lca node number
            // the document that corresponds
            // the frequency for that node and document.
            this->ll->insert(this->t->Preorder_Rank(lca)-1,i-1,da->countRange(i-1,vl_lca,vr_lca));
        //    cout << "inserting node = " << this->t->Preorder_Rank(lca) << " doc = " << i-1 << " with freq = " << da->countRange(i-1,vl_lca,vr_lca) << endl;
            pos++;
            pairc = this->da->selectDocument(i-1,pos+1);
            count++;
      
            if (pos >= this->length)
                break;
        }
    }
}
void Topk::generateSequence() {
//    cout << "Constructing Sequences" << endl;
    this->max_freq = 0;
    vector<uint> frequencies;
    vector<uint> depth_sequence;
    size_t pointersize = 0;
    size_t bitmap_size = 0;
    // a 1 is set every time a node is being examinated. 
    // all 0's corresponds to different documents for the same node.
    BitString *bsmap = new BitString(100*this->number_of_nodes);
    // if is a leaf, mark it here
    BitString *bsleaf = new BitString(100*this->number_of_nodes);
    // map leaf is to map the cst indexing to the pre-order traversal.
    BitString *map_leaf = new BitString(100*this->number_of_nodes);
    size_t docs_node = 0;
    size_t docs_aux = 0; 
    size_t docs_acum = 0 ;      
    size_t vl_p,vr_p;
    for (uint i = 0; i < this->number_of_nodes;i++) {
        bsmap->setBit(i+depth_sequence.size());
        pair<uint,uint> aux_node = this->preorder_vector[i];
       // cout << aux_node.first << ", " << aux_node.second << endl;
        if (aux_node.first == aux_node.second) {
        //    cout << "found leaf on node = " << i << endl;
            // cout << "current pos = " << pointersize+docs_node << endl;
            map_leaf->setBit(i);
            bsleaf->setBit(i+depth_sequence.size());
            uint node = this->t->Preorden_Select(nodes_preorder[make_pair(aux_node.first,aux_node.second)]);
            size_t tdepth = cst->TDepth(aux_node.first,aux_node.second);
          //  cout << "tdepth 1 = " << tdepth << endl;
            uint tdepth2 = this->t->Depth(node);
          //  cout << "tdepth 2 = " << tdepth2 << endl;
            depth_sequence.push_back(tdepth2);
            frequencies.push_back(1);
            this->documents.push_back(this->da->doc_array[aux_node.first]);
            continue;
        }
         // cout << "node = " << i << endl;
        size_t k = 0;
        // get the i-th node, the k-th document.
        // the document is aux.first
        // the frequency of that node, for that document is aux.second

        pair<uint,uint> aux = this->ll->getValue(i,k);
        pair<uint,uint> node_aux = preorder_vector[i];

        while(aux.second != (uint)-1) {
            uint level = 0;
            size_t vl,vr;
            vl = node_aux.first;
            vr = node_aux.second;
            bool cont = true;
            while(cont) {
            //    cout << "entered with node = " << i << endl;
                cst->Parent(vl,vr,&vl_p,&vr_p);
                // level++;
                // there is NO parent with a higher frequency...
                if (vl_p == (size_t)-1 || vr_p == (size_t)-1) {
                    if(i == 0) {
                        // cout << "Damn, node " << i << " has no parent" << endl;
                        uint l = 0;
                        pair<uint,uint> aux2 = ll->getValue(nodes_preorder[make_pair(vl_p,vr_p)],l);
                        while(aux2.second != (uint)-1 && aux.first != (uint)-1 && cont == true) {
                            this->documents.push_back(aux.first);
                            frequencies.push_back(aux.second);
                            depth_sequence.push_back(0);
                            if (aux.second > max_freq)
                                this->max_freq = aux.second;
                            l++; // move to the next document
                            aux2 = ll->getValue(nodes_preorder[make_pair(vl_p,vr_p)],l);
                        }
                        cont = false;
                        break;
                    } else {
              //          cout << "Damn, node " << i << " has no parent" << endl;
                        cont = false;
                        break;
                    }
                }

                size_t l = 0;
                // get the document_id and frequency of the parent (aux2)
                pair<uint,uint> aux2 = ll->getValue(nodes_preorder[make_pair(vl_p,vr_p)],l);
                // while(aux2.second != (uint)-1 && aux2.first != (uint)-1 && cont == true) {
                while(cont == true) {
                    if (aux2.first == (uint)-1) {
                //        cout << "aux2.first = " << aux2.first << " aux2.first = " << aux2.first;
                  //      cout << " vl_p = " << vl_p << " vr_p = " << vr_p << " l = " << l << endl; 
                    
                        l = 0;
                        size_t p1r, p1l;
                        this->cst->Parent(vl_p, vr_p, &p1l,  &p1r);
                        if (p1l != (size_t)-1 && p1r != (size_t)-1) {
                            aux2 = ll->getValue(nodes_preorder[make_pair(p1l, p1r)], l);
                        } else {
                            cont = false;
                        }
                        vl_p = p1l;
                        vr_p = p1r;
                        continue;
                    }       
                    if (aux2.first == aux.first) { // if they share the same document
                    //    if (i == 61) {
                      //      cout << "aux.second = " << aux.second << " aux2.second = " << aux2.second << endl;
                       // }
                        if (aux2.second >= aux.second) { // if the frequency of the parent is higher
                            uint parent = this->t->Parent(this->t->Preorden_Select(this->nodes_preorder[make_pair(vl,vr)]+1));
                            size_t tdepth = this->t->Depth(parent); // calculate the tree depth
                            depth_sequence.push_back(tdepth); // add the Tree Depth (0 is reserved for the dummy ones)
                            this->documents.push_back(aux.first); // add the document 
                            frequencies.push_back(aux.second); // add the frequency
                            //pointersize++; // increase the amount of pointers
                            docs_node++;
                            pointersize+=2;
                            // cout << "docs_node = " << docs_node << endl;
                            if (aux.second > max_freq)
                                this->max_freq = aux.second; 

                            cont = false;
                         //   break;
                        }
                    }
                    l++; // move to the next document
                    aux2 = ll->getValue(nodes_preorder[make_pair(vl_p,vr_p)],l); // retrieve the next document from the parent.
                }
                vl = vl_p; // set the initial values, to get the parent of the parent and so on...
                vr = vr_p;
            }
            k++; // get the next document,freq of the node being examinated
            aux = this->ll->getValue(i,k); 
        }
      //  docs_node++;

    }

    uint *bsmap_data =  bsmap->getData();  
    uint *bsleaf_data = bsleaf->getData();
    uint *leaf_data = map_leaf->getData();
    //  for (int i = 0 ; i < this->number_of_nodes+depth_sequence.size();i++) {
    //      cout << "bsmap[" << i << "] = " << bsmap->getBit(i) << " | " << bsleaf->getBit(i) << endl;
    //  }
    // cout << "done printing!" << endl;
    // cout << "creating new bitstrings" << endl;
    // resize the bitmaps to their corresponding sizes.
    bsmap = new BitString(bsmap_data,this->number_of_nodes+depth_sequence.size()+1);
    bsleaf = new BitString(bsleaf_data,this->number_of_nodes+depth_sequence.size()+1);
    map_leaf = new BitString(leaf_data,this->number_of_nodes+2);
    // cout << "setting last bit " << endl;
    bsmap->setBit(this->number_of_nodes+depth_sequence.size());
    bsleaf->setBit(this->number_of_nodes+depth_sequence.size());
    map_leaf->setBit(this->number_of_nodes);
    // cout << "constructing bitsequence" << endl;
    this->bitsequence_map = new BitSequenceRG(*bsmap,20);
    this->bitsequence_leaf = new BitSequenceRG(*bsleaf,20);
    this->bitmap_leaf = new BitSequenceRG(*map_leaf,20);
    // cout << "done!" << endl;
    nodes_preorder.clear();
    this->pointer_size = depth_sequence.size();
    uint *depth_sequence_array = new uint[depth_sequence.size()];
    // cout << "hardcopying the depth sequences " << endl;
    // hard copy depth_sequence
    this->freq_array = new uint[depth_sequence.size()];
    uint *norm_weight = new uint[depth_sequence.size()];

    for (size_t i = 0 ; i < depth_sequence.size();i++) {
         depth_sequence_array[i] = depth_sequence[i];
	 this->freq_array[i] = frequencies[i];
//	 if(this->max_freq <= frequencies[i]); {
     //   cout << "wtf! max_freq = " << this->max_freq << " vs freq[" << i << "] = " << frequencies[i] << endl;
  //   }
	 norm_weight[i] = this->max_freq - frequencies[i];
    }
    // cout << "done!" << endl;
    // cout << "max freq is = " << this->max_freq << endl;
    // depth sequences
    // cout << "pointer_size = " << this->pointer_size << endl;
    this->gd_sequence = &depth_sequence[0];
//    this->freq_array = &frequencies[0];
    uint *document_array = new uint[documents.size()];
    copy(documents.begin(), documents.end(), document_array);

    // norm_weight is the normalized weight (reversed orderder), to use it with the RMQ
//    uint *norm_weight = new uint[this->pointer_size ];
    // cout << "pointer_size = " << this->pointer_size << endl;
    // cout << "depth_sequence size = " << depth_sequence.size() << endl;
  //  std::map<uint,uint> freq_map; // for statistics
   // std::map<uint,uint>::const_iterator it;

    // for (it = freq_map.begin(); it != freq_map.end(); ++it) {
    //     cout << it->first << "\t" << it->second << endl;
    // }

    // cout << "Sorting Freqs and documents..." << endl;
    // using stable sort, to match to the leafs of the wavelet tree
    assert(documents.size() == depth_sequence.size());
    sort(this->gd_sequence,this->freq_array,document_array,depth_sequence.size());
    // cout << "Done!" << endl;
    // for (int i = 0 ; i < this->pointer_size;i++ )
    // {
     //    cout << "| " << i << " | " << gd_sequence[i] << " | " <<  this->freq_array[i] << " | " << document_array[i] << endl;
    // }
    this->doc_array = new Array(document_array,this->pointer_size);
    this->freq_dacs = new factorization(this->freq_array,this->pointer_size);

    frequencies.clear();
    // cout << "Constructing Wavelet Tree of Depth Sequences" << endl;
    Array *A = new Array(depth_sequence_array,this->pointer_size);
    MapperNone * map = new MapperNone();
    BitSequenceBuilder * bsb = new BitSequenceBuilderRG(20);
    this->d_sequence = new WaveletTreeRMQ(*A, bsb, map,norm_weight);

    // for (int i = 0; i < A->getLength(); i++)
    //     assert(A->getField(i) == d_sequence->access(i));

  //  for (int i = 0; i < A->getLength(); i++) {
  //      cout << i << " " << ((this->max_freq - norm_weight[i]) == (this->freq_array[i])) << " " << gd_sequence[i] << " " << (this->max_freq - norm_weight[i]) << " " << (this->freq_array[i]) << endl;
  //  }
    
    delete A;
    delete []norm_weight;
    delete this->da;
    delete this->ll;
    delete this->stp;
    delete bsmap;
    delete bsleaf;
  //  cout << "Done! " << endl;
}

double Topk::query(uchar *q,uint size_q) {
    //cout << "recevied query:" << q << " of length  = " << size_q << endl;
    clock_t begin=clock();
    pair<int, int> posn = ticsa->count(q,size_q);
    if (posn.second - posn.first + 1 == 0) {
        clock_t end=clock();
        return double(diffclock(end,begin));
    }
  //   cout << "numocc = " << posn.second - posn.first + 1 << endl;
    size_t start_range = posn.first;
    size_t end_range = posn.second;
    if (end_range > length) end_range = length;
    // cout << "start_range = " << start_range << endl;
    // cout << "end_range = " << end_range << endl;
    // // if is a leaf:
    if (start_range == end_range ) {
         uint result = this->d_array[this->bitsequence_leaf->select1(start_range)];
         clock_t end=clock();
        return double(diffclock(end,begin));
    }

    uint l1 = this->bitmap_leaf->select1(start_range);
    uint l2 = this->bitmap_leaf->select1(end_range);
    uint l11 = this->t->Preorden_Select(l1 + 1);
    uint l22 =  this->t->Preorden_Select(l2 + 1);
    uint lca = this->t->Lca(l11,l22);
    uint tdepth;
    uint p;
    uint pp;
    p = this->t->Preorder_Rank(lca);
    tdepth = this->t->Depth(lca);
    pp = p + this->t->Subtree_Size(lca);
    size_t s_new_range = this->bitsequence_map->select1(p) - p + 1;
    assert(pp <= this->bitsequence_map->countOnes());
    size_t e_new_range = this->bitsequence_map->select1(pp) - pp;
    uint max = 0;
    uint max_pos = 0;
    // for (int i  = s_new_range;i<=e_new_range;i++) {
    //     uint depth_test = this->d_sequence->access(i);
    //     if (depth_test >= 0 && depth_test <= tdepth) {
    //         if (max < this->freq_array[i]) {
    //             max = this->freq_array[i];
    //             max_pos = i;
    //         }
    //     }
    // }
    // cout << "MAX FREQ = " << max << " IN POS = " << max_pos << endl;


    vector<pair<uint,uint>> v = this->d_sequence->range_call(s_new_range, e_new_range, 1, tdepth, 10);
    // cout << "vector size = " << v.size() << endl;


    // for (int i = 0 ; i < v.size();i++) {
    //     cout << "v[" << i << "].weight = " << this->max_freq - v[i].first << endl;
    //     cout << "v[" << i << "].pos = " << v[i].second << endl;
    //     cout << "v[" << i << "].doc = " << this->doc_array->getField(v[i].second) << endl;
    // //    cout << "v[" << i << "].weight' = " << this->freq_array[v[i].second] << endl;
    // //    cout << "v[" << i << "].depth' = " << this->gd_sequence[v[i].second] << endl;
    // }
    clock_t end=clock();
    return double(diffclock(end,begin));
}


Topk::~Topk() {
    delete this->cst;
    delete[] this->freq_array;
    delete this->ticsa;
    delete this->d_sequence;
    //delete[] this->gd_sequence;
    delete this->t;
    delete []this->d_array;
    delete this->freq_dacs;
    delete this->bitsequence_leaf;
    delete this->bitsequence_map;
    delete this->d;
}


size_t Topk::getSize() {
    size_t cst_size = this->cst->getSize();
    size_t csa_size = this->ticsa->getSize();
    size_t wt_size = this->d_sequence->getSize();
    size_t freq_array_size = this->freq_dacs->getSize();
    size_t tree_size = this->t->size();
    size_t map_size = this->bitsequence_map->getSize() + this->bitmap_leaf->getSize();
    size_t documents_size = this->doc_array->getSize();
    
    size_t total = csa_size + wt_size + freq_array_size + map_size + tree_size + documents_size;
    
    cout << "CSA SIZE \t WT SIZE \t FREQ ARRAY \t TREE SIZE \t MAP SIZE \t DOCUMENT ARRAY \t TOTAL \t TOTAL(MB) \t RATIO " << endl;
    cout << csa_size << "\t" << wt_size << "\t" << freq_array_size << "\t" << tree_size << "\t" << map_size << "\t" << documents_size << "\t" << total << "\t" << total/(1024.00*1024.00) << "\t" << (total*1.0)/(this->length/1.0) << "\t" << endl;
    return total;

}
