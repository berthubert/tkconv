require 'formula'

class Catdoc < Formula  
  url 'http://ftp.wagner.pp.ru/pub/catdoc/catdoc-0.95.tar.gz'
  homepage 'http://wagner.pp.ru/~vitus/software/catdoc/'
  sha256 '514a84180352b6bf367c1d2499819dfa82b60d8c45777432fa643a5ed7d80796'  

  def install    

  # catdoc configure says it respects --mandir=, but does not.    
  ENV['man1dir'] = man1
  system "./configure --disable-debug --disable-dependency-tracking --prefix=#{prefix}"    

  # The INSTALL file confuses make on case insensitive filesystems.    
  system "mv INSTALL INSTALL.txt"
  system "make"    

  # There is a race condition in the charsets/Makefile install target. The following line solves it.    
  system "make -C charsets install-dirs"   
  system "make install"  
  end
end
