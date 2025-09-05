<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0"
  xmlns:vv="http://www.tweedekamer.nl/ggm/vergaderverslag/v1.0"
  exclude-result-prefixes="vv"
>
  <xsl:output method="html"
    version="5.0"
    doctype-system="about:legacy-compat"
    encoding="UTF-8"
    indent="yes"
  />

  <!-- This XSLT converts Dutch parliamentary documents into HTML. The input vocabulary is
  documented in https://github.com/TweedeKamerDerStaten-Generaal/OpenDataPortaal/tree/master/xsd/vlos -->

  <xsl:template match="/*">
          <xsl:apply-templates select="*" />
  </xsl:template>

  <xsl:template match="vv:vergadering">
    <h1>
      <xsl:value-of select="vv:titel" />
    </h1>
    <xsl:apply-templates select="vv:activiteit" />
  </xsl:template>

  <xsl:template match="vv:activiteit">
    <section class="activiteit">
      <xsl:apply-templates select="vv:activiteithoofd" />
    </section>
  </xsl:template>

  <xsl:template match="vv:activiteithoofd">
    <section class="activiteithoofd">
      <h2>
        <xsl:value-of select="vv:titel" />
      </h2>
      <xsl:apply-templates select="vv:tekst|vv:activiteitdeel" />
    </section>
  </xsl:template>

  <xsl:template match="vv:tekst">
    <section class="tekst">
      <xsl:apply-templates select="*" />
    </section>
  </xsl:template>

  <xsl:template match="vv:draadboekfragment">
    <section class="motie">
      <xsl:apply-templates select="vv:tekst" />
    </section>
  </xsl:template>


  <xsl:template match="vv:activiteitdeel">
    <section class="activiteitdeel">
      <xsl:apply-templates select="vv:tekst|vv:activiteititem" />
    </section>
  </xsl:template>

  <xsl:template match="vv:activiteititem">
    <section class="activiteititem">
      <xsl:apply-templates select="vv:tekst|vv:woordvoerder" />
    </section>
  </xsl:template>

  <xsl:template match="vv:woordvoerder">
    <section class="woordvoerder">
      <xsl:apply-templates select="vv:tekst|vv:interrumpant|vv:draadboekfragment" />
    </section>
  </xsl:template>

  <xsl:template match="vv:interrumpant">
    <section class="interrumpant">
      <xsl:apply-templates select="vv:tekst" />
    </section>
  </xsl:template>

  <xsl:template match="vv:nadruk[@type='Vet']">
    <strong>
      <xsl:apply-templates />
    </strong>
  </xsl:template>

  <xsl:template match="vv:nadruk[@type='Schuin']">
    <em>
      <xsl:apply-templates />
    </em>
  </xsl:template>

  <xsl:template match="vv:nadruk[@type='Bovenschrift']">
    <sup>
      <xsl:apply-templates />
    </sup>
  </xsl:template>

  <xsl:template match="vv:nadruk[@type='Onderschrift']">
    <sub>
      <xsl:apply-templates />
    </sub>
  </xsl:template>

  <xsl:template match="vv:dossiernummer|vv:stuknummer">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="vv:alinea">
    <div class="alinea">
      <xsl:apply-templates select="*" />
    </div>
  </xsl:template>

  <xsl:template match="vv:alinea/vv:alineaitem">
    <p>
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="vv:lijst">
    <ul>
      <xsl:apply-templates />
    </ul>
  </xsl:template>

  <xsl:template match="vv:lijst/vv:alineaitem">
    <li>
      <xsl:apply-templates />
    </li>
  </xsl:template>

  <xsl:template match="vv:vergadering" mode="title">
    <xsl:choose>
      <xsl:when test="@soort='Commissie'">Commissievergadering</xsl:when>
      <xsl:when test="@soort='Plenair'">Plenaire vergadering</xsl:when>
      <xsl:otherwise>Vergadering</xsl:otherwise>
    </xsl:choose>
    <xsl:text>; </xsl:text>
    <xsl:value-of
      select="vv:titel" />
    <!-- What else do we want in the title?-->
  </xsl:template>

  <!-- Helps the author of this xslt to see what still needs to be done -->
  <!-- <xsl:template match="vv:*">
    <xsl:message terminate="no">
      <xsl:text>Unhandled element </xsl:text>
      <xsl:value-of select="local-name()" />
    </xsl:message>
    <xsl:apply-templates />
  </xsl:template> -->
</xsl:stylesheet>