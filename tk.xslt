<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0"
  xmlns:vv="http://www.tweedekamer.nl/ggm/vergaderverslag/v1.0"
>
<xsl:output method="html" encoding="UTF-8" />

<xsl:template match="/">
<xsl:text disable-output-escaping="yes">&lt;!DOCTYPE html&gt;</xsl:text>
<html>
	<body>
    	<xsl:apply-templates select="//vv:alineaitem"/>
	</body>
</html>
</xsl:template>

  <!-- change nadruk to strong -->
  <xsl:template match="vv:nadruk">
    <strong>
      <xsl:copy-of select="./text()" />
    </strong>
  </xsl:template>

  <!-- change alineaitem to p -->
  <xsl:template match="vv:alineaitem">
    <p>
      <xsl:apply-templates select="node() | @*"/>  
    </p>
  </xsl:template>

                                                                                                                                                           
  <xsl:template match="node() | @*">                                                                                                                                                                    
    <xsl:copy>                                                                                                                                                                                          
      <xsl:apply-templates select="node() | @*"/>  
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>
