#include <cstring>
#include <fstream>
#include <mummer/nucmer.hpp>
#include <mummer/sparseSA.hpp>
#include <mummer/mgaps.hh>
#include <mummer/postnuc.hh>


namespace mummer {
namespace nucmer {

typedef postnuc::Synteny<FastaRecordSeq> synteny_type;

static inline char rc(const char c) {
  switch(c) {
  case 'a': return 't';
  case 'c': return 'g';
  case 'g': return 'c';
  case 't': return 'a';
  case 'A': return 'T';
  case 'C': return 'G';
  case 'G': return 'C';
  case 'T': return 'A';
  }
  return 'n';
}
static void reverse_complement(std::string& s) {
  auto st = s.begin();
  auto en = s.end() - 1;

  for( ; st < en; ++st, --en) {
    const char rc_st = rc(*st);
    *st = rc(*en);
    *en = rc_st;
  }
  if(st == en)
    *st = rc(*st);
}

const std::vector<std::string> SequenceAligner::descr { "" };
const std::vector<long> SequenceAligner::startpos { 0 };

void SequenceAligner::align(const char* query, std::vector<postnuc::Alignment>& alignments) {
  std::vector<mgaps::Match_t> fwd_matches(1), bwd_matches(1);
  FastaRecordSeq              Query(query);
  std::vector<synteny_type>   syntenys;
  syntenys.push_back(&Ref);
  synteny_type&               synteny = syntenys.front();
  mgaps::UnionFind            UF;
  char                        cluster_dir;

  auto append_cluster = [&](const mgaps::cluster_type& cluster) {
    postnuc::Cluster cl(cluster_dir);
    cl.matches.push_back({ cluster[0].Start1, cluster[0].Start2, cluster[0].Len });
    for(size_t i = 1; i < cluster.size(); ++i) {
      const auto& m = cluster[i];
      cl.matches.push_back({ m.Start1 + m.Simple_Adj, m.Start2 + m.Simple_Adj, m.Len - m.Simple_Adj });
    }
    synteny.clusters.push_back(std::move(cl));
  };
  if(options.orientation & FORWARD) {
    auto append_matches = [&](const mummer::match_t& m) { fwd_matches.push_back({ m.ref + 1, m.query + 1, m.len }); };
    switch(options.match) {
    case MUM: sa.findMUM_each(query, options.min_len, true, append_matches); break;
    case MUMREFERENCE: sa.findMAM_each(query, options.min_len, true, append_matches); break;
    case MAXMATCH: sa.findMEM_each(query, options.min_len, true, append_matches); break;
    }
    cluster_dir = postnuc::FORWARD_CHAR;
    clusterer.Cluster_each(fwd_matches.data(), UF, fwd_matches.size() - 1, append_cluster);
  }

  if(options.orientation & REVERSE) {
    std::string rquery(query);
    reverse_complement(rquery);
    auto append_matches = [&](const mummer::match_t& m) { bwd_matches.push_back({ m.ref + 1, m.query + 1, m.len }); };
    switch(options.match) {
    case MUM: sa.findMUM_each(rquery, options.min_len, true, append_matches); break;
    case MUMREFERENCE: sa.findMAM_each(rquery, options.min_len, true, append_matches); break;
    case MAXMATCH: sa.findMEM_each(rquery, options.min_len, true, append_matches); break;
    }
    cluster_dir = postnuc::REVERSE_CHAR;
    clusterer.Cluster_each(bwd_matches.data(), UF, bwd_matches.size() - 1, append_cluster);
  }

  merger.processSyntenys_each(syntenys, Query,
                              [&](std::vector<postnuc::Alignment>&& als, const FastaRecordSeq& Af,
                                  const FastaRecordSeq& Bf) { for(auto& al : als) alignments.push_back(std::move(al)); });
}
} // namespace nucmer
} // namespace mummer
