(set-info :source |fuzzsmt|)
(set-info :smt-lib-version 2.0)
(set-info :category "random")
(set-info :status unknown)
(set-logic QF_IDL)
(declare-fun v0 () Int)
(declare-fun v1 () Int)
(assert (let ((e2 5))
(let ((e3 1))
(let ((e4 14))
(let ((e5 3))
(let ((e6 1))
(let ((e7 (distinct v1 v0)))
(let ((e8 (> v1 v0)))
(let ((e9 (<= v1 v1)))
(let ((e10 (<= v0 v0)))
(let ((e11 (<= (- v0 v1) (- e4))))
(let ((e12 (> (- v1 v1) e4)))
(let ((e13 (> v0 v0)))
(let ((e14 (= (- v1 v0) (- e5))))
(let ((e15 (= (- v1 v0) e5)))
(let ((e16 (>= (- v1 v1) (- e4))))
(let ((e17 (<= v1 v1)))
(let ((e18 (< (- v0 v0) (- e5))))
(let ((e19 (= (- v1 v1) (- e4))))
(let ((e20 (>= (- v1 v0) (- e3))))
(let ((e21 (> v0 v1)))
(let ((e22 (>= (- v0 v0) e5)))
(let ((e23 (> v0 v1)))
(let ((e24 (>= (- v0 v1) (- e5))))
(let ((e25 (>= v1 v0)))
(let ((e26 (= (- v0 v1) e3)))
(let ((e27 (< v0 v0)))
(let ((e28 (< v1 v1)))
(let ((e29 (> v1 v0)))
(let ((e30 (distinct v1 v1)))
(let ((e31 (<= v0 v0)))
(let ((e32 (= v0 v1)))
(let ((e33 (> (- v0 v0) e3)))
(let ((e34 (<= (- v1 v1) (- e4))))
(let ((e35 (< (- v1 v1) e5)))
(let ((e36 (<= v1 v0)))
(let ((e37 (> v0 v1)))
(let ((e38 (<= (- v1 v1) e6)))
(let ((e39 (distinct v1 v0)))
(let ((e40 (= (- v0 v0) e2)))
(let ((e41 (> (- v1 v0) (- e3))))
(let ((e42 (<= (- v1 v0) (- e3))))
(let ((e43 (<= v0 v0)))
(let ((e44 (< (- v1 v0) e3)))
(let ((e45 (>= v1 v1)))
(let ((e46 (= (- v1 v0) e2)))
(let ((e47 (>= v0 v0)))
(let ((e48 (> v1 v0)))
(let ((e49 (>= (- v0 v1) (- e2))))
(let ((e50 (= (- v1 v0) e2)))
(let ((e51 (< (- v0 v1) e6)))
(let ((e52 (< v0 v0)))
(let ((e53 (= (- v0 v1) (- e3))))
(let ((e54 (distinct v0 v1)))
(let ((e55 (> (- v1 v0) (- e5))))
(let ((e56 (= (- v1 v0) (- e3))))
(let ((e57 (= v1 v0)))
(let ((e58 (< (- v0 v0) (- e5))))
(let ((e59 (distinct v1 v0)))
(let ((e60 (= v0 v1)))
(let ((e61 (< v0 v1)))
(let ((e62 (= (- v1 v0) (- e3))))
(let ((e63 (< v0 v0)))
(let ((e64 (> v1 v1)))
(let ((e65 (= v0 v0)))
(let ((e66 (distinct (- v1 v0) e5)))
(let ((e67 (< v0 v1)))
(let ((e68 (<= (- v1 v0) e5)))
(let ((e69 (distinct v1 v0)))
(let ((e70 (< (- v1 v1) (- e5))))
(let ((e71 (< v0 v0)))
(let ((e72 (< (- v0 v1) e3)))
(let ((e73 (<= (- v1 v0) e2)))
(let ((e74 (>= v0 v1)))
(let ((e75 (not e20)))
(let ((e76 (ite e8 e43 e56)))
(let ((e77 (=> e52 e30)))
(let ((e78 (and e22 e48)))
(let ((e79 (= e21 e41)))
(let ((e80 (or e12 e23)))
(let ((e81 (not e16)))
(let ((e82 (ite e32 e62 e18)))
(let ((e83 (ite e59 e77 e11)))
(let ((e84 (ite e68 e78 e17)))
(let ((e85 (= e28 e31)))
(let ((e86 (and e85 e53)))
(let ((e87 (or e25 e49)))
(let ((e88 (=> e47 e50)))
(let ((e89 (or e83 e73)))
(let ((e90 (not e63)))
(let ((e91 (=> e51 e45)))
(let ((e92 (xor e7 e57)))
(let ((e93 (xor e82 e72)))
(let ((e94 (or e35 e92)))
(let ((e95 (=> e76 e61)))
(let ((e96 (= e27 e95)))
(let ((e97 (not e96)))
(let ((e98 (=> e89 e89)))
(let ((e99 (not e54)))
(let ((e100 (ite e84 e64 e44)))
(let ((e101 (ite e87 e98 e60)))
(let ((e102 (xor e75 e33)))
(let ((e103 (or e67 e9)))
(let ((e104 (not e58)))
(let ((e105 (ite e14 e103 e104)))
(let ((e106 (and e71 e34)))
(let ((e107 (= e90 e105)))
(let ((e108 (not e13)))
(let ((e109 (ite e69 e46 e91)))
(let ((e110 (and e40 e102)))
(let ((e111 (and e42 e86)))
(let ((e112 (ite e37 e36 e10)))
(let ((e113 (xor e39 e97)))
(let ((e114 (ite e79 e79 e94)))
(let ((e115 (or e109 e93)))
(let ((e116 (or e38 e114)))
(let ((e117 (xor e88 e29)))
(let ((e118 (xor e117 e106)))
(let ((e119 (ite e15 e99 e101)))
(let ((e120 (=> e100 e119)))
(let ((e121 (or e24 e115)))
(let ((e122 (or e110 e107)))
(let ((e123 (not e74)))
(let ((e124 (xor e81 e113)))
(let ((e125 (ite e80 e26 e124)))
(let ((e126 (=> e108 e70)))
(let ((e127 (and e65 e125)))
(let ((e128 (= e118 e123)))
(let ((e129 (xor e120 e126)))
(let ((e130 (and e122 e127)))
(let ((e131 (or e112 e66)))
(let ((e132 (xor e111 e116)))
(let ((e133 (ite e121 e132 e19)))
(let ((e134 (xor e129 e129)))
(let ((e135 (and e131 e134)))
(let ((e136 (xor e128 e135)))
(let ((e137 (not e133)))
(let ((e138 (ite e55 e130 e136)))
(let ((e139 (and e138 e137)))
e139
)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(check-sat)
