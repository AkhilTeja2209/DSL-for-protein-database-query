// Live UniProt: reviewed human kinases, longest first.
// The executor fetches the REST response once and caches it under .cache/.

LOAD UNIPROT "kinase AND organism_id:9606 AND reviewed:true" TOP 100

FIND proteins
WHERE length > 800
SORT BY length DESC
TOP 10
DISPLAY proteinid name length function
