package eJavaGuruString2;

/*
class EJavaGuruString2 {
	public static void main(String args[]) {
		String ejg = "game".replace('a', 'Z').trim().concat("Aa");
		ejg.substring(0, 2);
		System.out.println(ejg);
	}
}
*/

class EJavaGuruString2 {
	public static void main(String args[]) {
		String ejg = "game";
		ejg.replace('a', 'Z').trim().concat("Aa");
		ejg = ejg.substring(0, 2);
		System.out.println(ejg);
	}
}

/*
 game->gZme->gZme->gZmeAa
 */