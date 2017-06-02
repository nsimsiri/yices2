(set-logic QF_UFIDL)
(set-info :source |
These benchmarks were generated by Panagiotis Manolios and Sudarshan K. Srinivasan.
They were generated from experiments in microprocessor verification based on
refinement.

This benchmark was automatically translated into SMT-LIB format by Albert Oliveras.
|)
(set-info :smt-lib-version 2.0)
(set-info :category "industrial")
(set-info :status unsat)
(declare-fun pc0 () Int)
(declare-fun a1 () Int)
(declare-fun ZERO () Int)
(declare-fun dmem0 () Int)
(declare-fun IMem0 (Int) Int)
(declare-fun GetImm (Int) Int)
(declare-fun DMem_Read (Int Int) Int)
(declare-fun op (Int) Int)
(declare-fun dest (Int) Int)
(declare-fun alu (Int Int Int) Int)
(declare-fun SelectTargetPC (Int Int Int) Int)
(declare-fun NextDMem (Int Int Int) Int)
(declare-fun src1 (Int) Int)
(declare-fun rf0 (Int) Int)
(declare-fun src2 (Int) Int)
(declare-fun TakeBranch (Int Int Int) Bool)
(declare-fun GetIsBranch (Int) Bool)
(declare-fun GetRegWrite (Int) Bool)
(declare-fun GetMemToReg (Int) Bool)
(declare-fun GetMemWrite (Int) Bool)
(declare-fun GetuseImm (Int) Bool)
(assert (let ((?v_0 (IMem0 pc0))) (let ((?v_13 (op ?v_0)) (?v_14 (rf0 (src1 ?v_0))) (?v_15 (rf0 (src2 ?v_0)))) (let ((?v_37 (and (TakeBranch ?v_13 ?v_14 ?v_15) (GetIsBranch ?v_0)))) (let ((?v_6 (not ?v_37)) (?v_12 (GetRegWrite ?v_0)) (?v_8 (+ 1 pc0))) (let ((?v_1 (IMem0 ?v_8))) (let ((?v_3 (src1 ?v_1)) (?v_2 (dest ?v_0)) (?v_4 (src2 ?v_1))) (let ((?v_7 (and ?v_12 (or (= ?v_3 ?v_2) (= ?v_4 ?v_2))))) (let ((?v_5 (not ?v_7))) (let ((?v_43 (and ?v_6 ?v_5)) (?v_35 (op ?v_1)) (?v_36 (rf0 ?v_3)) (?v_79 (rf0 ?v_4))) (let ((?v_34 (and ?v_43 (and (and (TakeBranch ?v_35 ?v_36 ?v_79) ?v_5) (GetIsBranch ?v_1))))) (let ((?v_25 (not ?v_34)) (?v_161 (GetRegWrite ?v_1)) (?v_22 (+ 1 ?v_8))) (let ((?v_24 (IMem0 ?v_22))) (let ((?v_9 (ite ?v_7 ?v_1 ?v_24))) (let ((?v_11 (src1 ?v_9)) (?v_10 (dest ?v_1)) (?v_17 (src2 ?v_9))) (let ((?v_23 (and (and ?v_161 ?v_5) (or (= ?v_11 ?v_10) (= ?v_17 ?v_10))))) (let ((?v_19 (and ?v_6 (not ?v_23)))) (let ((?v_44 (and ?v_25 ?v_19)) (?v_20 (op ?v_9)) (?v_16 (alu ?v_13 ?v_14 (ite (GetuseImm ?v_0) (GetImm ?v_0) ?v_15)))) (let ((?v_18 (ite (GetMemToReg ?v_0) (DMem_Read dmem0 ?v_16) ?v_16))) (let ((?v_21 (ite (and (= ?v_11 ?v_2) ?v_12) ?v_18 (rf0 ?v_11))) (?v_81 (ite (and (= ?v_17 ?v_2) ?v_12) ?v_18 (rf0 ?v_17)))) (let ((?v_46 (and ?v_44 (and (and (TakeBranch ?v_20 ?v_21 ?v_81) ?v_19) (GetIsBranch ?v_9)))) (?v_45 (ite ?v_7 ?v_8 ?v_22)) (?v_31 (+ 1 ?v_22))) (let ((?v_30 (IMem0 ?v_31))) (let ((?v_29 (ite ?v_7 ?v_24 ?v_30))) (let ((?v_26 (ite ?v_23 ?v_9 ?v_29))) (let ((?v_169 (GetRegWrite ?v_26)) (?v_224 (GetRegWrite ?v_9)) (?v_160 (src1 ?v_26)) (?v_27 (dest ?v_9)) (?v_166 (src2 ?v_26))) (let ((?v_28 (and (and ?v_224 ?v_19) (or (= ?v_160 ?v_27) (= ?v_166 ?v_27))))) (let ((?v_68 (not ?v_28))) (let ((?v_50 (and ?v_68 ?v_6))) (let ((?v_47 (and ?v_25 ?v_50)) (?v_38 (+ 1 ?v_31))) (let ((?v_94 (IMem0 ?v_38))) (let ((?v_105 (ite ?v_7 ?v_30 ?v_94))) (let ((?v_113 (ite ?v_23 ?v_29 ?v_105))) (let ((?v_32 (ite ?v_28 ?v_26 ?v_113))) (let ((?v_124 (src1 ?v_32)) (?v_33 (dest ?v_26)) (?v_126 (src2 ?v_32))) (let ((?v_49 (and (and ?v_169 ?v_47) (or (= ?v_124 ?v_33) (= ?v_126 ?v_33)))) (?v_273 (SelectTargetPC ?v_13 ?v_14 pc0)) (?v_58 (+ 1 ?v_38))) (let ((?v_39 (+ 1 ?v_58))) (let ((?v_40 (ite ?v_7 ?v_39 (+ 1 ?v_39)))) (let ((?v_41 (ite ?v_37 ?v_273 (ite ?v_23 ?v_40 (+ 1 ?v_40))))) (let ((?v_42 (ite ?v_34 (SelectTargetPC ?v_35 ?v_36 ?v_8) (ite ?v_28 ?v_41 (+ 1 ?v_41))))) (let ((?v_73 (ite ?v_46 (SelectTargetPC ?v_20 ?v_21 ?v_45) (ite ?v_49 ?v_42 (+ 1 ?v_42)))) (?v_48 (not ?v_46))) (let ((?v_85 (and ?v_48 ?v_47)) (?v_52 (ite ?v_7 ?v_22 ?v_31))) (let ((?v_51 (ite ?v_23 ?v_45 ?v_52)) (?v_67 (not ?v_49)) (?v_53 (and ?v_25 (or (and ?v_28 ?v_6) ?v_50)))) (let ((?v_54 (and ?v_67 ?v_53))) (let ((?v_83 (and ?v_48 ?v_54)) (?v_57 (ite ?v_7 ?v_31 ?v_38))) (let ((?v_56 (ite ?v_23 ?v_52 ?v_57))) (let ((?v_55 (ite ?v_28 ?v_51 ?v_56)) (?v_66 (and ?v_49 ?v_53))) (let ((?v_59 (and ?v_48 (or ?v_66 ?v_54))) (?v_62 (ite ?v_7 ?v_38 ?v_58))) (let ((?v_61 (ite ?v_23 ?v_57 ?v_62))) (let ((?v_60 (ite ?v_28 ?v_56 ?v_61))) (let ((?v_119 (ite ?v_49 ?v_55 ?v_60)) (?v_65 (ite ?v_7 ?v_58 ?v_39))) (let ((?v_64 (ite ?v_23 ?v_62 ?v_65))) (let ((?v_63 (ite ?v_28 ?v_61 ?v_64))) (let ((?v_112 (ite ?v_49 ?v_60 ?v_63)) (?v_70 (ite ?v_23 ?v_65 ?v_40))) (let ((?v_69 (ite ?v_28 ?v_64 ?v_70))) (let ((?v_104 (ite ?v_49 ?v_63 ?v_69)) (?v_71 (and ?v_25 (or ?v_68 ?v_6)))) (let ((?v_74 (and ?v_48 (or ?v_66 (and ?v_67 ?v_71)))) (?v_72 (ite ?v_28 ?v_70 ?v_41))) (let ((?v_93 (ite ?v_49 ?v_69 ?v_72)) (?v_77 (and ?v_48 (or ?v_67 ?v_71))) (?v_90 (ite ?v_49 ?v_72 ?v_42))) (let ((?v_89 (ite ?v_43 ?v_8 (ite ?v_44 ?v_45 (ite ?v_85 ?v_51 (ite ?v_83 ?v_55 (ite ?v_59 ?v_119 (ite ?v_59 ?v_112 (ite ?v_59 ?v_104 (ite ?v_74 ?v_93 (ite ?v_77 ?v_90 ?v_73)))))))))) (?v_75 (not ?v_74)) (?v_76 (not ?v_59)) (?v_78 (not ?v_77)) (?v_80 (ite (GetMemWrite ?v_0) (NextDMem dmem0 ?v_16 ?v_15) dmem0)) (?v_162 (alu ?v_35 ?v_36 (ite (GetuseImm ?v_1) (GetImm ?v_1) ?v_79)))) (let ((?v_82 (ite (and ?v_43 (GetMemWrite ?v_1)) (NextDMem ?v_80 ?v_162 ?v_79) ?v_80)) (?v_223 (alu ?v_20 ?v_21 (ite (GetuseImm ?v_9) (GetImm ?v_9) ?v_81)))) (let ((?v_196 (ite (and ?v_44 (GetMemWrite ?v_9)) (NextDMem ?v_82 ?v_223 ?v_81) ?v_82))) (let ((?v_91 (= ?v_196 ?v_80)) (?v_84 (not ?v_83)) (?v_86 (not ?v_85)) (?v_87 (not ?v_44)) (?v_88 (not ?v_43)) (?v_92 (+ 1 ?v_89)) (?v_95 (IMem0 ?v_58))) (let ((?v_96 (ite ?v_7 ?v_95 (IMem0 ?v_39)))) (let ((?v_97 (ite ?v_23 ?v_96 (IMem0 ?v_40)))) (let ((?v_98 (ite ?v_28 ?v_97 (IMem0 ?v_41)))) (let ((?v_100 (ite ?v_49 ?v_98 (IMem0 ?v_42))) (?v_99 (IMem0 ?v_89)) (?v_101 (+ 1 ?v_92)) (?v_106 (ite ?v_7 ?v_94 ?v_95))) (let ((?v_107 (ite ?v_23 ?v_106 ?v_96))) (let ((?v_108 (ite ?v_28 ?v_107 ?v_97))) (let ((?v_102 (ite ?v_49 ?v_108 ?v_98)) (?v_103 (IMem0 ?v_92)) (?v_109 (+ 1 ?v_101)) (?v_114 (ite ?v_23 ?v_105 ?v_106))) (let ((?v_115 (ite ?v_28 ?v_114 ?v_107))) (let ((?v_111 (ite ?v_49 ?v_115 ?v_108)) (?v_110 (IMem0 ?v_101)) (?v_116 (+ 1 ?v_109)) (?v_120 (ite ?v_28 ?v_113 ?v_114))) (let ((?v_118 (ite ?v_49 ?v_120 ?v_115)) (?v_117 (IMem0 ?v_109)) (?v_121 (+ 1 ?v_116)) (?v_122 (IMem0 ?v_116)) (?v_123 (ite ?v_49 ?v_32 ?v_120))) (let ((?v_139 (+ 1 ?v_121)) (?v_141 (IMem0 ?v_121)) (?v_143 (op ?v_32)) (?v_163 (op ?v_99)) (?v_144 (ite (and (= ?v_124 ?v_2) ?v_12) ?v_18 (rf0 ?v_124))) (?v_125 (src1 ?v_99))) (let ((?v_164 (ite (and (= ?v_125 ?v_2) ?v_12) ?v_18 (rf0 ?v_125))) (?v_146 (ite (and (= ?v_126 ?v_2) ?v_12) ?v_18 (rf0 ?v_126))) (?v_127 (src2 ?v_99))) (let ((?v_168 (ite (and (= ?v_127 ?v_2) ?v_12) ?v_18 (rf0 ?v_127))) (?v_148 (dest ?v_32)) (?v_138 (dest ?v_99)) (?v_149 (GetImm ?v_32)) (?v_174 (GetImm ?v_99)) (?v_129 (GetuseImm ?v_32))) (let ((?v_150 (not ?v_129)) (?v_128 (GetuseImm ?v_99)) (?v_131 (GetIsBranch ?v_32))) (let ((?v_152 (not ?v_131)) (?v_130 (GetIsBranch ?v_99)) (?v_133 (GetMemToReg ?v_32))) (let ((?v_154 (not ?v_133)) (?v_132 (GetMemToReg ?v_99))) (let ((?v_177 (not ?v_132)) (?v_135 (GetMemWrite ?v_32))) (let ((?v_156 (not ?v_135)) (?v_134 (GetMemWrite ?v_99))) (let ((?v_179 (not ?v_134)) (?v_137 (GetRegWrite ?v_32))) (let ((?v_158 (not ?v_137)) (?v_136 (GetRegWrite ?v_99))) (let ((?v_170 (not ?v_136)) (?v_145 (src1 ?v_103)) (?v_147 (src2 ?v_103))) (let ((?v_140 (and ?v_136 (or (= ?v_145 ?v_138) (= ?v_147 ?v_138))))) (let ((?v_184 (ite ?v_140 ?v_139 (+ 1 ?v_139))) (?v_187 (ite ?v_140 ?v_116 ?v_121)) (?v_189 (ite ?v_140 ?v_122 ?v_141)) (?v_191 (ite ?v_140 ?v_109 ?v_116)) (?v_192 (ite ?v_140 ?v_117 ?v_122)) (?v_194 (ite ?v_140 ?v_101 ?v_109)) (?v_195 (ite ?v_140 ?v_110 ?v_117)) (?v_188 (ite ?v_140 ?v_121 ?v_139)) (?v_190 (ite ?v_140 ?v_141 (IMem0 ?v_139))) (?v_198 (ite ?v_140 ?v_92 ?v_101)) (?v_182 (ite ?v_140 ?v_103 ?v_110)) (?v_142 (not ?v_140)) (?v_209 (op ?v_103)) (?v_210 (ite (and (= ?v_145 ?v_2) ?v_12) ?v_18 (rf0 ?v_145))) (?v_212 (ite (and (= ?v_147 ?v_2) ?v_12) ?v_18 (rf0 ?v_147))) (?v_183 (dest ?v_103)) (?v_216 (GetImm ?v_103)) (?v_151 (GetuseImm ?v_103)) (?v_153 (GetIsBranch ?v_103)) (?v_155 (GetMemToReg ?v_103))) (let ((?v_220 (not ?v_155)) (?v_157 (GetMemWrite ?v_103))) (let ((?v_222 (not ?v_157)) (?v_159 (GetRegWrite ?v_103))) (let ((?v_214 (not ?v_159)) (?v_171 (op ?v_26)) (?v_167 (ite (GetMemToReg ?v_1) (DMem_Read ?v_80 ?v_162) ?v_162))) (let ((?v_172 (ite (and (and ?v_43 (= ?v_160 ?v_10)) ?v_161) ?v_167 (ite (and (= ?v_160 ?v_2) ?v_12) ?v_18 (rf0 ?v_160))))) (let ((?v_208 (SelectTargetPC ?v_171 ?v_172 ?v_51)) (?v_181 (SelectTargetPC ?v_163 ?v_164 ?v_89))) (let ((?v_165 (= ?v_208 ?v_181)) (?v_173 (ite (and (and ?v_43 (= ?v_166 ?v_10)) ?v_161) ?v_167 (ite (and (= ?v_166 ?v_2) ?v_12) ?v_18 (rf0 ?v_166)))) (?v_213 (not ?v_169))) (let ((?v_215 (alu ?v_171 ?v_172 (ite (GetuseImm ?v_26) (GetImm ?v_26) ?v_173))) (?v_197 (alu ?v_163 ?v_164 (ite ?v_128 ?v_174 ?v_168))) (?v_176 (and (and (TakeBranch ?v_171 ?v_172 ?v_173) ?v_47) (GetIsBranch ?v_26)))) (let ((?v_217 (not ?v_176)) (?v_175 (and (TakeBranch ?v_163 ?v_164 ?v_168) ?v_130))) (let ((?v_185 (not ?v_175)) (?v_178 (GetMemToReg ?v_26))) (let ((?v_219 (not ?v_178)) (?v_180 (GetMemWrite ?v_26))) (let ((?v_221 (not ?v_180)) (?v_200 (src1 ?v_182)) (?v_201 (src2 ?v_182))) (let ((?v_186 (and (and ?v_159 ?v_142) (or (= ?v_200 ?v_183) (= ?v_201 ?v_183))))) (let ((?v_228 (ite ?v_175 ?v_181 (ite ?v_186 ?v_184 (+ 1 ?v_184)))) (?v_232 (ite ?v_186 ?v_187 ?v_188)) (?v_234 (ite ?v_186 ?v_189 ?v_190)) (?v_193 (and (or ?v_76 ?v_185) (or ?v_175 ?v_59))) (?v_237 (ite ?v_186 ?v_191 ?v_187)) (?v_238 (ite ?v_186 ?v_192 ?v_189)) (?v_240 (ite ?v_186 ?v_194 ?v_191)) (?v_241 (ite ?v_186 ?v_195 ?v_192)) (?v_233 (ite ?v_186 ?v_188 ?v_184)) (?v_235 (ite ?v_186 ?v_190 (IMem0 ?v_184))) (?v_245 (ite ?v_134 (NextDMem ?v_80 ?v_197 ?v_168) ?v_80)) (?v_247 (ite ?v_186 ?v_198 ?v_194)) (?v_226 (ite ?v_186 ?v_182 ?v_195)) (?v_199 (and ?v_185 (not ?v_186))) (?v_258 (op ?v_182)) (?v_260 (ite (and (= ?v_200 ?v_2) ?v_12) ?v_18 (rf0 ?v_200))) (?v_262 (ite (and (= ?v_201 ?v_2) ?v_12) ?v_18 (rf0 ?v_201))) (?v_227 (dest ?v_182)) (?v_265 (GetImm ?v_182)) (?v_202 (GetuseImm ?v_182)) (?v_203 (GetIsBranch ?v_182)) (?v_204 (GetMemToReg ?v_182))) (let ((?v_268 (not ?v_204)) (?v_205 (GetMemWrite ?v_182))) (let ((?v_269 (not ?v_205)) (?v_206 (GetRegWrite ?v_182))) (let ((?v_263 (not ?v_206)) (?v_207 (and ?v_185 ?v_142))) (let ((?v_270 (not ?v_207)) (?v_225 (SelectTargetPC ?v_209 ?v_210 ?v_92))) (let ((?v_211 (= ?v_208 ?v_225)) (?v_246 (alu ?v_209 ?v_210 (ite ?v_151 ?v_216 ?v_212))) (?v_218 (and (and (TakeBranch ?v_209 ?v_210 ?v_212) ?v_142) ?v_153)) (?v_271 (ite (GetMemToReg ?v_9) (DMem_Read ?v_82 ?v_223) ?v_223)) (?v_259 (ite ?v_132 (DMem_Read ?v_80 ?v_197) ?v_197)) (?v_272 (not ?v_224))) (let ((?v_229 (and ?v_207 ?v_218)) (?v_250 (src1 ?v_226)) (?v_251 (src2 ?v_226))) (let ((?v_230 (and (and ?v_206 ?v_199) (or (= ?v_250 ?v_227) (= ?v_251 ?v_227)))) (?v_242 (not ?v_229))) (let ((?v_243 (not ?v_230))) (let ((?v_248 (and ?v_243 ?v_185))) (let ((?v_231 (and ?v_242 (or (and ?v_230 ?v_185) ?v_248)))) (let ((?v_236 (not ?v_231))) (let ((?v_239 (and (or ?v_76 ?v_231) (or ?v_236 ?v_59))) (?v_244 (and ?v_242 (or ?v_243 ?v_185))) (?v_249 (and ?v_242 ?v_248)) (?v_252 (GetuseImm ?v_226)) (?v_253 (GetIsBranch ?v_226)) (?v_254 (GetMemToReg ?v_226)) (?v_255 (GetMemWrite ?v_226)) (?v_256 (GetRegWrite ?v_226)) (?v_257 (and ?v_242 ?v_199)) (?v_264 (ite (and (= ?v_200 ?v_138) ?v_136) ?v_259 ?v_260))) (let ((?v_261 (= ?v_208 (SelectTargetPC ?v_258 ?v_264 ?v_198))) (?v_266 (ite (and (= ?v_201 ?v_138) ?v_136) ?v_259 ?v_262))) (let ((?v_267 (and (and (TakeBranch ?v_258 ?v_264 ?v_266) ?v_199) ?v_203)) (?v_276 (ite ?v_37 ?v_273 ?v_8)) (?v_274 (rf0 a1))) (let ((?v_277 (= (ite (and (= a1 ?v_2) ?v_12) ?v_18 ?v_274) ?v_274)) (?v_278 (= ?v_80 dmem0)) (?v_279 (+ 1 ZERO))) (let ((?v_280 (+ 1 ?v_279))) (let ((?v_281 (+ 1 ?v_280))) (let ((?v_282 (+ 1 ?v_281))) (let ((?v_283 (+ 1 ?v_282))) (let ((?v_284 (+ 1 ?v_283))) (let ((?v_275 (+ 1 ?v_284))) (let ((?v_285 (+ 1 (+ 1 (+ 1 (+ 1 ?v_275)))))) (not (and (and (or (or (or (or (or (or (or (or (or (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 ?v_89) ?v_75) ?v_75) ?v_76) ?v_76) ?v_76) ?v_76) ?v_78) ?v_78) ?v_91) ?v_76) ?v_76) ?v_84) ?v_84) ?v_86) ?v_86) ?v_87) ?v_87) ?v_88) ?v_88) (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 ?v_92) ?v_75) ?v_75) ?v_76) ?v_76) ?v_76) ?v_76) ?v_77) (or ?v_78 (and (= ?v_90 ?v_89) (= ?v_100 ?v_99)))) ?v_91) ?v_76) ?v_76) ?v_84) ?v_84) ?v_86) ?v_86) ?v_87) ?v_87) ?v_88) ?v_88)) (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 ?v_101) ?v_74) (or ?v_75 (and (= ?v_93 ?v_89) (= ?v_102 ?v_99)))) ?v_76) ?v_76) ?v_76) ?v_76) ?v_77) (or ?v_78 (and (= ?v_90 ?v_92) (= ?v_100 ?v_103)))) ?v_91) ?v_76) ?v_76) ?v_84) ?v_84) ?v_86) ?v_86) ?v_87) ?v_87) ?v_88) ?v_88)) (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 ?v_109) ?v_74) (or ?v_75 (and (= ?v_93 ?v_92) (= ?v_102 ?v_103)))) ?v_59) (or ?v_76 (and (= ?v_104 ?v_89) (= ?v_111 ?v_99)))) ?v_76) ?v_76) ?v_77) (or ?v_78 (and (= ?v_90 ?v_101) (= ?v_100 ?v_110)))) ?v_91) ?v_76) ?v_76) ?v_84) ?v_84) ?v_86) ?v_86) ?v_87) ?v_87) ?v_88) ?v_88)) (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 ?v_116) ?v_74) (or ?v_75 (and (= ?v_93 ?v_101) (= ?v_102 ?v_110)))) ?v_59) (or ?v_76 (and (= ?v_104 ?v_92) (= ?v_111 ?v_103)))) ?v_59) (or ?v_76 (and (= ?v_112 ?v_89) (= ?v_118 ?v_99)))) ?v_77) (or ?v_78 (and (= ?v_90 ?v_109) (= ?v_100 ?v_117)))) ?v_91) ?v_76) ?v_76) ?v_84) ?v_84) ?v_86) ?v_86) ?v_87) ?v_87) ?v_88) ?v_88)) (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 ?v_121) ?v_74) (or ?v_75 (and (= ?v_93 ?v_109) (= ?v_102 ?v_117)))) ?v_59) (or ?v_76 (and (= ?v_104 ?v_101) (= ?v_111 ?v_110)))) ?v_59) (or ?v_76 (and (= ?v_112 ?v_92) (= ?v_118 ?v_103)))) ?v_77) (or ?v_78 (and (= ?v_90 ?v_116) (= ?v_100 ?v_122)))) ?v_91) ?v_59) (or ?v_76 (and (= ?v_119 ?v_89) (= ?v_123 ?v_99)))) ?v_84) ?v_84) ?v_86) ?v_86) ?v_87) ?v_87) ?v_88) ?v_88)) (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 ?v_139) ?v_74) (or ?v_75 (and (= ?v_93 ?v_116) (= ?v_102 ?v_122)))) ?v_59) (or ?v_76 (and (= ?v_104 ?v_109) (= ?v_111 ?v_117)))) ?v_59) (or ?v_76 (and (= ?v_112 ?v_101) (= ?v_118 ?v_110)))) ?v_77) (or ?v_78 (and (= ?v_90 ?v_121) (= ?v_100 ?v_141)))) ?v_91) ?v_59) (or ?v_76 (and (= ?v_119 ?v_92) (= ?v_123 ?v_103)))) ?v_83) (or ?v_84 (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_55 ?v_89) (= ?v_143 ?v_163)) (= ?v_144 ?v_164)) (= ?v_146 ?v_168)) (= ?v_148 ?v_138)) (= ?v_124 ?v_125)) (= ?v_126 ?v_127)) (= ?v_149 ?v_174)) (and (or ?v_150 ?v_128) (or (not ?v_128) ?v_129))) (and (or ?v_152 ?v_130) (or (not ?v_130) ?v_131))) (and (or ?v_154 ?v_132) (or ?v_177 ?v_133))) (and (or ?v_156 ?v_134) (or ?v_179 ?v_135))) (and (or ?v_158 ?v_136) (or ?v_170 ?v_137))))) ?v_86) ?v_86) ?v_87) ?v_87) ?v_88) ?v_88)) (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 ?v_184) ?v_74) (or ?v_75 (and (= ?v_93 ?v_187) (= ?v_102 ?v_189)))) ?v_59) (or ?v_76 (and (= ?v_104 ?v_191) (= ?v_111 ?v_192)))) ?v_59) (or ?v_76 (and (= ?v_112 ?v_194) (= ?v_118 ?v_195)))) ?v_77) (or ?v_78 (and (= ?v_90 ?v_188) (= ?v_100 ?v_190)))) ?v_91) ?v_59) (or ?v_76 (and (= ?v_119 ?v_198) (= ?v_123 ?v_182)))) (and (or ?v_84 ?v_142) (or ?v_140 ?v_83))) (or ?v_84 (and (and (and (and (and (and (and (and (and (and (and (and (and ?v_142 (= ?v_55 ?v_92)) (= ?v_143 ?v_209)) (= ?v_144 ?v_210)) (= ?v_146 ?v_212)) (= ?v_148 ?v_183)) (= ?v_124 ?v_145)) (= ?v_126 ?v_147)) (= ?v_149 ?v_216)) (and (or ?v_150 ?v_151) (or (not ?v_151) ?v_129))) (and (or ?v_152 ?v_153) (or (not ?v_153) ?v_131))) (and (or ?v_154 ?v_155) (or ?v_220 ?v_133))) (and (or ?v_156 ?v_157) (or ?v_222 ?v_135))) (and (or ?v_158 ?v_159) (or ?v_214 ?v_137))))) ?v_85) (or ?v_86 (and (and (and (and (and (and (and (and (and ?v_165 (= ?v_33 ?v_138)) (= ?v_51 ?v_89)) ?v_165) (= ?v_173 ?v_168)) (and (or ?v_213 ?v_136) (or ?v_170 ?v_169))) (= ?v_215 ?v_197)) (and (or ?v_217 ?v_175) (or ?v_185 ?v_176))) (and (or ?v_219 ?v_132) (or ?v_177 ?v_178))) (and (or ?v_221 ?v_134) (or ?v_179 ?v_180))))) ?v_87) ?v_87) ?v_88) ?v_88)) (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 ?v_228) (and (or ?v_75 ?v_185) (or ?v_175 ?v_74))) (or ?v_75 (and (and ?v_185 (= ?v_93 ?v_232)) (= ?v_102 ?v_234)))) ?v_193) (or ?v_76 (and (and ?v_185 (= ?v_104 ?v_237)) (= ?v_111 ?v_238)))) ?v_193) (or ?v_76 (and (and ?v_185 (= ?v_112 ?v_240)) (= ?v_118 ?v_241)))) (and (or ?v_78 ?v_185) (or ?v_175 ?v_77))) (or ?v_78 (and (and ?v_185 (= ?v_90 ?v_233)) (= ?v_100 ?v_235)))) (= ?v_196 ?v_245)) ?v_193) (or ?v_76 (and (and ?v_185 (= ?v_119 ?v_247)) (= ?v_123 ?v_226)))) (and (or ?v_84 ?v_199) (or (not ?v_199) ?v_83))) (or ?v_84 (and (and (and (and (and (and (and (and (and (and (and (and (and ?v_199 (= ?v_55 ?v_198)) (= ?v_143 ?v_258)) (= ?v_144 ?v_260)) (= ?v_146 ?v_262)) (= ?v_148 ?v_227)) (= ?v_124 ?v_200)) (= ?v_126 ?v_201)) (= ?v_149 ?v_265)) (and (or ?v_150 ?v_202) (or (not ?v_202) ?v_129))) (and (or ?v_152 ?v_203) (or (not ?v_203) ?v_131))) (and (or ?v_154 ?v_204) (or ?v_268 ?v_133))) (and (or ?v_156 ?v_205) (or ?v_269 ?v_135))) (and (or ?v_158 ?v_206) (or ?v_263 ?v_137))))) (and (or ?v_86 ?v_207) (or ?v_270 ?v_85))) (or ?v_86 (and (and (and (and (and (and (and (and (and (and ?v_207 ?v_211) (= ?v_33 ?v_183)) (= ?v_51 ?v_92)) ?v_211) (= ?v_173 ?v_212)) (and (or ?v_213 ?v_159) (or ?v_214 ?v_169))) (= ?v_215 ?v_246)) (and (or ?v_217 ?v_218) (or (not ?v_218) ?v_176))) (and (or ?v_219 ?v_155) (or ?v_220 ?v_178))) (and (or ?v_221 ?v_157) (or ?v_222 ?v_180))))) ?v_44) (or ?v_87 (and (and (= ?v_271 ?v_259) (= ?v_27 ?v_138)) (and (or ?v_272 ?v_136) (or ?v_170 ?v_224))))) ?v_88) ?v_88)) (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (and (= ?v_73 (ite ?v_229 ?v_225 (ite ?v_230 ?v_228 (+ 1 ?v_228)))) (and (or ?v_75 ?v_231) (or ?v_236 ?v_74))) (or ?v_75 (and (and ?v_231 (= ?v_93 (ite ?v_230 ?v_232 ?v_233))) (= ?v_102 (ite ?v_230 ?v_234 ?v_235))))) ?v_239) (or ?v_76 (and (and ?v_231 (= ?v_104 (ite ?v_230 ?v_237 ?v_232))) (= ?v_111 (ite ?v_230 ?v_238 ?v_234))))) ?v_239) (or ?v_76 (and (and ?v_231 (= ?v_112 (ite ?v_230 ?v_240 ?v_237))) (= ?v_118 (ite ?v_230 ?v_241 ?v_238))))) (and (or ?v_78 ?v_244) (or (not ?v_244) ?v_77))) (or ?v_78 (and (and ?v_244 (= ?v_90 (ite ?v_230 ?v_233 ?v_228))) (= ?v_100 (ite ?v_230 ?v_235 (IMem0 ?v_228)))))) (= ?v_196 (ite (and ?v_207 ?v_157) (NextDMem ?v_245 ?v_246 ?v_212) ?v_245))) ?v_239) (or ?v_76 (and (and ?v_231 (= ?v_119 (ite ?v_230 ?v_247 ?v_240))) (= ?v_123 (ite ?v_230 ?v_226 ?v_241))))) (and (or ?v_84 ?v_249) (or (not ?v_249) ?v_83))) (or ?v_84 (and (and (and (and (and (and (and (and (and (and (and (and (and ?v_249 (= ?v_55 ?v_247)) (= ?v_143 (op ?v_226))) (= ?v_144 (ite (and (= ?v_250 ?v_2) ?v_12) ?v_18 (rf0 ?v_250)))) (= ?v_146 (ite (and (= ?v_251 ?v_2) ?v_12) ?v_18 (rf0 ?v_251)))) (= ?v_148 (dest ?v_226))) (= ?v_124 ?v_250)) (= ?v_126 ?v_251)) (= ?v_149 (GetImm ?v_226))) (and (or ?v_150 ?v_252) (or (not ?v_252) ?v_129))) (and (or ?v_152 ?v_253) (or (not ?v_253) ?v_131))) (and (or ?v_154 ?v_254) (or (not ?v_254) ?v_133))) (and (or ?v_156 ?v_255) (or (not ?v_255) ?v_135))) (and (or ?v_158 ?v_256) (or (not ?v_256) ?v_137))))) (and (or ?v_86 ?v_257) (or (not ?v_257) ?v_85))) (or ?v_86 (and (and (and (and (and (and (and (and (and (and ?v_257 ?v_261) (= ?v_33 ?v_227)) (= ?v_51 ?v_198)) ?v_261) (= ?v_173 ?v_266)) (and (or ?v_213 ?v_206) (or ?v_263 ?v_169))) (= ?v_215 (alu ?v_258 ?v_264 (ite ?v_202 ?v_265 ?v_266)))) (and (or ?v_217 ?v_267) (or (not ?v_267) ?v_176))) (and (or ?v_219 ?v_204) (or ?v_268 ?v_178))) (and (or ?v_221 ?v_205) (or ?v_269 ?v_180))))) (and (or ?v_87 ?v_207) (or ?v_270 ?v_44))) (or ?v_87 (and (and (and ?v_207 (= ?v_271 (ite ?v_155 (DMem_Read ?v_245 ?v_246) ?v_246))) (= ?v_27 ?v_183)) (and (or ?v_272 ?v_159) (or ?v_214 ?v_224))))) ?v_43) (or ?v_88 (and (and (= ?v_167 ?v_259) (= ?v_10 ?v_138)) (and (or (not ?v_161) ?v_136) (or ?v_170 ?v_161)))))) (or (and (and (= ?v_276 pc0) ?v_277) ?v_278) (< ?v_275 ?v_285))) (or (= ?v_276 ?v_89) (and (and (and (= pc0 ?v_89) ?v_277) ?v_278) (< (ite ?v_43 ZERO (ite ?v_44 ?v_279 (ite ?v_85 ?v_280 (ite ?v_83 ?v_281 (ite ?v_59 ?v_282 (ite ?v_59 ?v_283 (ite ?v_59 ?v_284 (ite ?v_74 ?v_275 (ite ?v_77 ?v_285 (+ 1 ?v_285)))))))))) ZERO))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
(check-sat)
(exit)