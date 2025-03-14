--source include/elide_costs.inc

SET EXPLAIN_FORMAT=tree;

CREATE TABLE t1 (f1 INT, f2 INT, f3 CHAR(1), f4 CHAR(1), f5 CHAR(1), f6 CHAR(1), f7 CHAR(1),
PRIMARY KEY (f5, f1), KEY (f2), KEY (f3), KEY (f4), KEY(f7) );
INSERT INTO t1 VALUES (1, 1, 'a', 'h', 'i', '', ''), (2, 3, 'a', 'h', 'i', '', ''),
                      (3, 2, 'b', '', 'j', '', ''), (4, 2, 'b', '', 'j', '', '');
INSERT INTO t1 VALUES (5, 1, 'a', 'h', 'i', '', ''), (6, 3, 'a', 'h', 'i', '', ''),
                      (7, 2, 'b', '', 'j', '', ''), (8, 2, 'b', '', 'j', '', '');
INSERT INTO t1 SELECT f1 + 8, f2, f3, f4, f5, f6, f7 FROM t1;
INSERT INTO t1 SELECT f1 + 16, f2, f3, f4, f5, f6, f7 FROM t1;
INSERT INTO t1 VALUES (33, 3, 'c', 'g', '', '', ''), (34, 4, 'c', 'g', '', '', ''),
                      (35, 5, 'd', 'f', '', '', ''), (36, 6, 'd', 'f', '', '', '');
INSERT INTO t1 SELECT f1 + 36, f2, f3, f4, f5, f6, f7 FROM t1;
INSERT INTO t1 SELECT f1 + 72, f2, f3, f4, f5, f6, f7 FROM t1;
INSERT INTO t1 SELECT f1 + 144, f2, f3, f4, f5, f6, f7 FROM t1;
ANALYZE TABLE t1;

# Index merge intersection without hints.
--replace_regex $elide_costs
EXPLAIN SELECT COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2;
--replace_regex $elide_costs
EXPLAIN SELECT COUNT(*) FROM t1 WHERE f4 = 'h' AND f3 = 'b';
--replace_regex $elide_costs
EXPLAIN SELECT COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# Index merge sort union without hints.
--replace_regex $elide_costs
EXPLAIN SELECT * FROM t1 WHERE f2 BETWEEN 5 AND 6 OR f3 = 'c' OR f4 = 'f';
--replace_regex $elide_costs
EXPLAIN SELECT * FROM t1 WHERE (f2 BETWEEN 5 AND 6 OR f3 = 'c') AND (f2 BETWEEN 5 AND 6 OR f4 = 'f');
# Index merge union without hints.
--replace_regex $elide_costs
EXPLAIN SELECT f1 FROM t1 WHERE f2 = 5 OR f3 = 'c' OR f4 = 'f';

# Check behavior of duplicated hints.
# First specified hint is applied and next conflicting hints are ignored with warning.

# Hint INDEX_MERGE(t1 f2, f3, f4) is ignored as duplicated.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) INDEX_MERGE(t1 f2, f3, f4) */ f2 FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# Hint NO_INDEX_MERGE(t1) is ignored as duplicated.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3, f4) INDEX_MERGE(t1) */ f2 FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# Hint NO_INDEX_MERGE(t1) is ignored as duplicated.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3, f4) NO_INDEX_MERGE(t1) */ f2 FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# Hint INDEX_MERGE(t1 f2, f3, f4) is ignored as duplicated.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1) INDEX_MERGE(t1 f2, f3, f4) */ f2 FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# Hint INDEX_MERGE(t1 f2, f3, f4) is ignored as duplicated.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f2, f4) INDEX_MERGE(t1 f2, f3, f4) */ f2 FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';

# Check behavior of unresolved hints.
# Hints with unresolved argument is ignored with warning.

# Hint is ignored, since table 't5' does not exist.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t5) */ f2 FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# Hint is ignored, since there is no f5 index in the table.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f5) */ f2 FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# Hint is ignored, since there are no f6, f5 indexes in the table.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f6, f5, primary) */ f2 FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# Hint is ignored, since table 't2' does not exist and there are no f9, f10 indexes in the table.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t2 f9, f10) */ COUNT(*) FROM t1 WHERE f4 = '' AND f2 = 2 AND f3 = 'b';
# Hint is ignored, since there are no f9, f10 indexes in the table.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f9, f10) */ COUNT(*) FROM t1 WHERE f4 = '' AND f2 = 2 AND f3 = 'b';

# Index merge intersection
# Turn off automatic use of index merge intersection, so that we get to
# test that the hints below are effective.
SET optimizer_switch='index_merge_intersection=off';
# intersect(f2,f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f4, f2) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2;
# intersect(f2,f3,f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f4, f3) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# intersect(f3,f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f4, f3) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# intersect(f2,f3,f4), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';

# Index merge with clustered key
# intersect(f3,f4), since 'f4, f3' indexes are specified in the hints and
# there is no condition for index 'f2'.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f4, f3) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f3 = 'b' AND f5 = 'i';
# intersect(f3,f4), since it's covering index merge and PRIMARY index is ignored in this case.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 PRIMARY, f4, f3) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f3 = 'b' AND f5 = 'i';

# no index merge, since 'index_merge_intersection' optimizer switch is off.
--replace_regex $elide_costs
EXPLAIN SELECT count(*) FROM t1 WHERE f2 = 3 AND f5 > '' AND f3 = 'c';
# intersect(f3, PRIMARY), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f3, PRIMARY) */ count(*) FROM t1 WHERE f2 = 3 AND f5 > '' AND f3 = 'c';
# intersect(f2, PRIMARY), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, PRIMARY) */ count(*) FROM t1 WHERE f2 = 3 AND f5 > '' AND f3 = 'c';
# Hints is ignored due to unresolved index f5. 'f5' index does not exist in the table.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f4, f5) */ f3 FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';

# intersect(f4,f3), since it's the cheapest intersection.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ COUNT(*) FROM t1 WHERE f4 = 'd' AND f2 = 2 AND f3 = 'b';
# intersect(f2,f3,f4), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# intersect(f2,f3,f4,f7), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3, f4, f7) */ COUNT(*) FROM t1 WHERE f4 = '' AND f2 = 2 AND f3 = 'b' AND f7 = '';
# intersect(f4,f3), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ COUNT(*) FROM t1 WHERE f4 = 'd' AND f2 = 2 AND f3 = 'b' AND f7 = '';
# intersect(f2,f3), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ COUNT(*) FROM t1 WHERE f4 = '' AND f2 = 2 AND f3 = 'b' AND f7 = '';

SET optimizer_switch='index_merge=off';
# no index merge, since 'index_merge' optimizer switch is off.
--replace_regex $elide_costs
EXPLAIN SELECT COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# intersect(f2, f3, f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3, f4) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
SET optimizer_switch='index_merge=on';

# No index merge intersection
SET optimizer_switch='index_merge_intersection=on';
# no index merge, since ref access by key 'f2' is the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f4) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2;
# no index merge, since there is no applicable indexes for index merge due to NO_INDEX_MERGE(t1) hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# intersect(f4, f2), since it's the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f3) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2;
# no index merge, since ref access by key 'f2' is the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f4) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';

# NO_INDEX_MERGE with clustered key
# intersect(f3, PRIMARY), since it's the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT count(*) FROM t1 WHERE f2 = 3 AND f5 > '' AND f3 = 'c';
# no index merge, since range access by 'f3' keys is the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 PRIMARY) */ count(*) FROM t1 WHERE f2 = 3 AND f5 > '' AND f3 = 'c';
# intersect(f3, PRIMARY), since it's the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f2) */ count(*) FROM t1 WHERE f2 = 3 AND f5 > '' AND f3 = 'c';
# no index merge, since ref access by 'f3' index is the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f3) */ count(*) FROM t1 WHERE f2 = 3 AND f5 > '' AND f3 = 'c';


# no index merge, since ref access by 'f3' index is the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT COUNT(*) FROM t1 WHERE f4 = 'x' AND f2 = 5 AND f3 = 'n';
# intersect(f3,f4), since it's the cheapest intersection.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ COUNT(*) FROM t1 WHERE f4 = 'x' AND f2 = 5 AND f3 = 'n';
# intersect(f4,f2), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f4, f2) */ COUNT(*) FROM t1 WHERE f4 = 'x' AND f2 = 5 AND f3 = 'n';
# intersect(f3,f2), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f3, f2) */ COUNT(*) FROM t1 WHERE f4 = 'x' AND f2 = 5 AND f3 = 'n';
# intersect(f4,f3), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f4, f3) */ COUNT(*) FROM t1 WHERE f4 = 'x' AND f2 = 5 AND f3 = 'n';

# no intersection, since not-equal condition can not be used for intersection.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f4) */ COUNT(*) FROM t1 WHERE f4 = 'h' AND f2 > 2;

# Index merge union
SET optimizer_switch='index_merge_union=off,index_merge=off';
# union(f2,f3,f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3, f4) */ f1 FROM t1 WHERE f2 = 5 OR f3 = 'c' OR f4 = 'f';
# no union because hint does not apply to where clause.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3) */ f1 FROM t1 WHERE f2 = 5 OR f3 = 'c' OR f4 = 'f';
# union(f2,f3), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3) */ f1 FROM t1 WHERE (f2 = 5 OR f3 = 'c') AND (f1 = 4 OR f4 = 'f');
# union(PRIMARY,f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 PRIMARY, f4) */ f1 FROM t1 WHERE (f2 = 5 OR f3 = 'c') AND (f5 = 'i' OR f4 = 'f');

SET optimizer_switch='index_merge_union=on,index_merge=on';
# no union, since indexes 'f2', 'f3', 'f4' can not be used for index merge due to NO_INDEX_MERGE hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f2, f3, f4) */ f1 FROM t1 WHERE f2 = 5 OR f3 = 'c' OR f4 = 'f';
# no union, since indexes 'f2', 'f3' can not be used for index merge due to NO_INDEX_MERGE hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f2, f3) */ f1 FROM t1 WHERE (f2 = 5 OR f3 = 'c') AND (f1 = 4 OR f4 = 'f');
# union(f2,f3), since index 'f4' can not be used for index merge due to NO_INDEX_MERGE hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f4) */ f1 FROM t1 WHERE (f2 = 5 OR f3 = 'c') AND (f1 = 4 OR f4 = 'f');

# no union, since ref access by 'f2' index is cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT * FROM t1 WHERE f2 = 400 AND (f3 = 'x' OR f4 = 'n');
# union(f3,f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f3, f4) */ * FROM t1 WHERE f2 = 400 AND (f3 = 'x' OR f4 = 'n');

# Index merge sort union
SET optimizer_switch='index_merge_sort_union=off,index_merge=off';
# sort_union(f2, f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f4) */ * FROM t1 WHERE (f2 BETWEEN 5 AND 6 OR f3 = 'c') AND (f2 BETWEEN 5 AND 6 OR f4 = 'f');
# sort_union(f2, f3), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3) */ * FROM t1 WHERE (f2 BETWEEN 5 AND 6 OR f3 = 'c') AND (f2 BETWEEN 5 AND 6 OR f4 = 'f');
# no sort_union, since hints with one specified index is ignored.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f3) */ * FROM t1 WHERE (f2 BETWEEN 5 AND 6 OR f3 = 'c') AND (f2 BETWEEN 5 AND 6 OR f4 = 'f');
# sort_union(f2, f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f4) */ * FROM t1
WHERE (f2 BETWEEN 5 AND 6 OR f3 = 'c') AND (f2 BETWEEN 5 AND 6 OR f4 = 'f') AND f3 = 'n';

SET optimizer_switch='index_merge_sort_union=on,index_merge=on';
# no sort_union, since indexes 'f2', 'f4' can not be used for index merge due to NO_INDEX_MERGE hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f2, f4) */ * FROM t1 WHERE (f2 BETWEEN 5 AND 6 OR f3 = 'c') AND (f2 BETWEEN 5 AND 6 OR f4 = 'f');
# no sort_union, since indexes 'f2', 'f3' can not be used for index merge due to NO_INDEX_MERGE hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f2, f3) */ * FROM t1 WHERE (f2 BETWEEN 5 AND 6 OR f3 = 'c') AND (f2 BETWEEN 5 AND 6 OR f4 = 'f');
# sort_union(f2, f3), since index 'f4' can not be used for index merge due to NO_INDEX_MERGE hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f4) */ * FROM t1 WHERE (f2 BETWEEN 5 AND 6 OR f3 = 'c') AND (f2 BETWEEN 5 AND 6 OR f4 = 'f');
# sort_union(f2, f4), since index 'f3' can not be used for index merge due to NO_INDEX_MERGE hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX_MERGE(t1 f3) */ * FROM t1 WHERE (f2 BETWEEN 5 AND 6 OR f3 = 'c') AND (f2 BETWEEN 5 AND 6 OR f4 = 'f');

# no sort_union, since full scan is the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT * FROM t1 WHERE (f2 BETWEEN 1 AND 200 OR f3 = 'c') AND (f2 BETWEEN 1 AND 200 OR f4 = 'f');
# sort_union(f2, f3), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ * FROM t1 WHERE (f2 BETWEEN 1 AND 200 OR f3 = 'c') AND (f2 BETWEEN 1 AND 200 OR f4 = 'f');
# sort_union(f2, f4), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f4) */ * FROM t1 WHERE (f2 BETWEEN 1 AND 200 OR f3 = 'c') AND (f2 BETWEEN 1 AND 200 OR f4 = 'f');

# union(f2,f3,f4), since it's the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT f1 FROM t1 WHERE (f2 = 5 OR f3 = 'c' OR f4 = 'f') AND (f2 BETWEEN 1 AND 200 OR f3 = 'c');
# sort_union(f2,f3), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3) */ f1 FROM t1 WHERE (f2 = 5 OR f3 = 'c' OR f4 = 'f') AND (f2 BETWEEN 1 AND 200 OR f3 = 'c');

# Tests for INDEX_MERGE hint with no index specified.
SET optimizer_switch=default;
# union(f3,f7), since it's the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT COUNT(*) FROM t1 WHERE (f4 = 'h' AND f2 = 2 AND f3 = 'b') OR (f7 = 'd' AND f6 = 'e');
# union(f3,f7), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ COUNT(*) FROM t1 WHERE (f4 = 'h' AND f2 = 2 AND f3 = 'b') OR (f7 = 'd' AND f6 = 'e');
# union(intersect(f2,f3),f7), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2,f3,f7) */ COUNT(*) FROM t1 WHERE (f4 = 'h' AND f2 = 2 AND f3 = 'b') OR (f7 = 'd' AND f6 = 'e');
# Demonstrates EXPLAIN ANALYZE (and thus, also the tree format) of unions of intersections. The filter is redundant.
--replace_regex $elide_time
EXPLAIN ANALYZE SELECT /*+ INDEX_MERGE(t1 f2,f3,f7) */ COUNT(*) FROM t1 WHERE (f4 = 'h' AND f2 = 2 AND f3 = 'b') OR (f7 = 'd' AND f6 = 'e');
# intersect(f4,PRIMARY), since it's the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT f3 FROM t1 WHERE f4 = 'f' AND f2 = 3 AND f5 > '';
# intersect(f4,PRIMARY), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ f3 FROM t1 WHERE f4 = 'f' AND f2 = 3 AND f5 > '';
# intersect(f2,f4), since it's the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT f3 FROM t1 WHERE f2 = 3 AND f4 = 'g' AND f5 = '';
# intersect(f2,f4), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ f3 FROM t1 WHERE f2 = 3 AND f4 = 'g' AND f5 = '';
# no index merge, since range by 'f2' key is the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT f3 FROM t1 WHERE f2 = 3 AND f4 = '' AND f5 > '';
# intersect(f2,PRIMARY), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ f3 FROM t1 WHERE f2 = 3 AND f4 = '' AND f5 > '';
# union(f2,f7), since it's the cheapest access method.
--replace_regex $elide_costs
EXPLAIN SELECT f3 FROM t1 WHERE (f2 = 3 AND f4 = '' AND f5 > '') OR (f7 = 'd' AND f6 = 'e');
# union(f2,f7), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ f3 FROM t1 WHERE (f2 = 3 AND f4 = '' AND f5 > '') OR (f7 = 'd' AND f6 = 'e');
# union(intersect(f2,PRIMARY),f7), since only indexes specified in the hint are applicable for index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, PRIMARY, f7) */ f3 FROM t1 WHERE (f2 = 3 AND f4 = '' AND f5 > '') OR (f7 = 'd' AND f6 = 'e');
# intersect(f2,f4), since it's the cheapest index merge.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ f2 FROM t1 WHERE f2 = 2 AND f4 = 'g' AND f5 = '';

# Test with IGNORE/FORCE INDEX
# no union, since only 'f4' index can be used for index merge due to 'IGNORE INDEX' hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f3, f4) */ * FROM t1 IGNORE INDEX (f3) WHERE f2 = 400 AND (f3 = 'x' OR f4 = 'n');
# no union, since only 'f3' index can be used for index merge due to 'FORCE INDEX' hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f3, f4) */ * FROM t1 FORCE INDEX (f3) WHERE f2 = 400 AND (f3 = 'x' OR f4 = 'n');
# no sort_union, since only 'f2' index can be used for index merge due to 'IGNORE INDEX' hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f4) */ * FROM t1 IGNORE INDEX (f4)
WHERE (f2 BETWEEN 1 AND 200 OR f3 = 'c') AND (f2 BETWEEN 1 AND 200 OR f4 = 'f');
# no sort_union, since only 'f4' index can be used for index merge due to 'FORCE INDEX' hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f4) */ * FROM t1 FORCE INDEX (f4)
WHERE (f2 BETWEEN 1 AND 200 OR f3 = 'c') AND (f2 BETWEEN 1 AND 200 OR f4 = 'f');
# no index merge, since indexes 'f2', 'f3' can not be used for intersection due to 'IGNORE INDEX' hint.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3, f4) */ f2 FROM t1 IGNORE INDEX (f2, f3) WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# intersect(f2, f3), since specified in 'FROCE INDEX' indexes only can be used for intersection.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3, f4) */ f2 FROM t1 FORCE INDEX (f2, f3) WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';
# intersect(f2, f3), since specified in 'USE INDEX' indexes only can be used for intersection.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3, f4) */ f2 FROM t1 USE INDEX (f2, f3) WHERE f4 = 'h' AND f2 = 2 AND f3 = 'b';


ALTER TABLE t1 ADD KEY idx(f3, f4);
--echo # Original query
--replace_regex $elide_costs
EXPLAIN SELECT f2 FROM t1 WHERE f2 = 2 AND f3 = 'b' AND f4 = 'h';
--echo # Index merge intersection(f2, f3, f4) should be used.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1 f2, f3, f4) */ f2 FROM t1 WHERE f2 = 2 AND f3 = 'b' AND f4 = 'h';


SET optimizer_switch= default;
DROP TABLE t1;

--echo #
--echo # Bug#24788119 GET_BEST_DISJUNCT_QUICK() - MYSQLD GOT SIGNAL 11
--echo #

CREATE TABLE t1(f1 INT NOT NULL, f2 INT, f3 INT, PRIMARY KEY(f1), KEY(f2), KEY(f3));
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ f1 FROM t1 WHERE f1 < 2 AND (f3 = 3 or f2 = 3);
DROP TABLE t1;

--echo #
--echo # Bug#24834724 QUICK_ROR_INTERSECT_SELECT::GET_NEXT() - MYSQLD GOT SIGNAL 11
--echo #

CREATE TABLE t1 (
  f1 VARCHAR(10) DEFAULT NULL,
  f2 INT(11) NOT NULL,
  f3 INT(11) DEFAULT NULL,
  PRIMARY KEY (f2),
  KEY f1 (f1),
  KEY f3 (f3)
);

INSERT INTO t1 VALUES ('b',1,NULL), ('h',5,NULL);

SELECT /*+ INDEX_MERGE(t1 f3, primary) */ f2 FROM t1
WHERE f1 = 'o' AND f2 = f3 AND f3 <= 4;

DROP TABLE t1;
