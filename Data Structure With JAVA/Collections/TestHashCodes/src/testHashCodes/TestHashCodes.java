package testHashCodes;

public class TestHashCodes {
	public static void main(String[] args) {
		printHashCode("Rad");
		printHashCode("Uhr");
		printHashCode("Ohr");
		printHashCode("Tor");
		printHashCode("Hut");
		printHashCode("Tag");
	}
	
	private static void printHashCode(String word) {
		System.out.printf("%s: %s%n", word, word.hashCode()); 
	} 
}