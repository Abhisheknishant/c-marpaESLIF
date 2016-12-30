package org.parser.marpa;

public class ESLIFApp  {

	public static void main(String[] args) throws Exception {
		ESLIFLogger eslifLogger = new ESLIFLogger();

		ESLIF eslif = new ESLIF(eslifLogger);
		
		eslifLogger.info("marpaESLIF version is " + eslif.version());

		final String grammar = 
				    ":default ::= action => do_double\n"
				  + ":discard ::= whitespace\n"
				  + "Expression ::=\n"
				  + "    /[\\d]+/                          action => do_int\n"
				  + "    | '(' Expression ')'              assoc => group action => ::copy[1]\n"
				  + "   ||     Expression '**' Expression  assoc => right\n"
				  + "   ||     Expression  '*' Expression\n"
				  + "    |     Expression  '/' Expression\n"
				  + "   ||     Expression  '+' Expression\n"
				  + "    |     Expression  '-' Expression\n"
				  + "whitespace :[1]:= [\\s]\n"
				  + "\n";


		ESLIFGrammar eslifGrammar = new ESLIFGrammar(eslif, grammar);
		eslifLogger.info("number of grammars         : " + eslifGrammar.ngrammar());
		eslifLogger.info("current grammar level      : " + eslifGrammar.currentLevel());
		eslifLogger.info("current grammar description: " + eslifGrammar.currentDescription());
		for (int level = 0; level < eslifGrammar.ngrammar(); level++) {
			eslifLogger.info("- grammar description at level " + level + ": " + eslifGrammar.descriptionByLevel(level));;
		}
		eslifLogger.info("current grammar show:\n" + eslifGrammar.show());
		{
			int[] rules = eslifGrammar.currentRuleIds();
			for (int i = 0; i < rules.length; i++) {
				eslifLogger.info("- current rule No " + i + ": " + rules[i]); 
				eslifLogger.info("    display form: " + eslifGrammar.ruleDisplay(i)); 
				eslifLogger.info("    show    form: " + eslifGrammar.ruleShow(i)); 
			}
		}
		for (int level = 0; level < eslifGrammar.ngrammar(); level++) {
			eslifLogger.info("- grammar show at level: " + level + "\n:" + eslifGrammar.showByLevel(level));
			eslifLogger.info("- rule array at level " + level);
			int[] rules = eslifGrammar.ruleIdsByLevel(level);
			for (int i = 0; i < rules.length; i++) {
				eslifLogger.info("  + rule No " + i + ": " + rules[i]); 
				eslifLogger.info("    display form: " + eslifGrammar.ruleDisplayByLevel(level, i)); 
				eslifLogger.info("    show    form: " + eslifGrammar.ruleShowByLevel(level, i)); 
			}
		}
		eslifGrammar.free();

		eslif.free();
	}
}