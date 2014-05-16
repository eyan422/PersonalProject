package concordance;

public class TestConcordance {
	//public static final String PATH = "C:\\Users\\UFIS-Frank\\Desktop\\";
	public static final String PATH = "D:\\Git\\PersonalProject\\Data Structure With JAVA\\Collections\\Concordance\\src\\concordance\\";
	public static final String IN_FILE = "cases.txt";
	public static final String OUT_FILE = "cases.out";
	
	public static void main(String[] args) {
		Concordance c = new Concordance(PATH+IN_FILE);
		c.write(PATH+OUT_FILE);
	}
}
