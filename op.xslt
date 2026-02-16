<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:exsl="http://exslt.org/common"
  xmlns:func="http://exslt.org/functions"
  xmlns:str="http://exslt.org/strings"
  xmlns:saxon="http://icl.com/saxon"
  xmlns:smop="http://smop.org/xslt"
  extension-element-prefixes="exsl func str saxon smop"
  version="1.0"
>
  <xsl:output method="html"
    version="5.0"
    doctype-system="about:legacy-compat"
    encoding="UTF-8"
    indent="yes"
  />

  <!-- This XSLT converts Dutch Official Publication documents into HTML. The input vocabulary is
  documented in
  http://technische-documentatie.oep.overheid.nl/repository/schemas/op-consolidated/op-consolidated_2014-05-15/xsd/op-xsd-2014-05-15.xsd -->

  <xsl:template match="/*">
    <html>
      <head>
        <title>
          <xsl:apply-templates select="*" mode="title" />
        </title>
        <link
          rel="stylesheet"
          href="html/pico.min.css"
        />
        <link
          rel="stylesheet"
          href="html/op.css"
        />
        <style>
        </style>
      </head>
      <body>
        <main class="container">
          <xsl:apply-templates select="*" />
        </main>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="metadata|kamerstukkop|kamervraagkop|kamervraagnummer" />

  <xsl:template match="kamerstuk">
    <article>
      <xsl:apply-templates select="*" />
    </article>
  </xsl:template>

  <xsl:template match="kamervragen">
    <article>
      <xsl:apply-templates select="*" />
      <xsl:call-template name="voetnoten"/>
    </article>
  </xsl:template>
  
  <xsl:template match="dossier">
    <h1 class="dossiertitel">
      <xsl:apply-templates select="*" />
    </h1>
  </xsl:template>

  <xsl:template match="kamervraagomschrijving">
    <h1 class="kamervraagomschrijving">
      <xsl:apply-templates/>
    </h1>
  </xsl:template>
  
  <xsl:template match="dossiernr|dossier/titel|begrotingshoofdstuk|kamervraagonderwerp|organisatie">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="stuk">
    <div class="stuk">
      <h1 class="stuktitel">
        <xsl:apply-templates select="stuknr|titel" />
      </h1>
      <xsl:apply-templates select="datumtekst" />
      <xsl:apply-templates select="officiele-inhoudsopgave|voorstel-wet|amendement|algemeen" />
    </div>
    <xsl:call-template name="voetnoten"/>>
  </xsl:template>

  <xsl:template match="stuknr">
    <span class="stuknummer">
      <xsl:apply-templates />
    </span>
    <xsl:text> </xsl:text>
  </xsl:template>

  <xsl:template match="ondernummer|herdruk|stuk/titel|vrije-tekst|tekst|wijzig-artikel">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="noot">
    <xsl:apply-templates select="noot.nr" />
  </xsl:template>

  <xsl:template match="noot.nr">
    <sup>
      <a class="nootnum supernote-click-ID-{generate-id()}" href="#ID-{generate-id()}">
        <xsl:apply-templates />
      </a>
    </sup>
  </xsl:template>

  <xsl:template match="noot" mode="voetnoten">
    <div class="voet noot snp-mouseoffset snb-pinned notedefault"
      id="supernote-note-ID-{generate-id()}">
      <h5 class="note-close"><a class="note-close" href="#close-ID-{generate-id()}">X</a> Noot </h5>
      <xsl:apply-templates mode="voetnoten" />
    </div>
  </xsl:template>

  <xsl:template match="noot.nr" mode="voetnoten">
    <sup>
      <span class="nootnum" id="ID-{generate-id()}">
        <xsl:apply-templates />
      </span>
    </sup>
  </xsl:template>

  <xsl:template match="noot.al" mode="voetnoten">
    <p>
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="datumtekst">
    <p class="kamerstukdatum">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template
    match="amendement|amendement-lid|algemeen|voorstel-wet|aanhef|wettekst|
    wijzig-lid|wijzig-divisie|wijziging|voorstel-sluiting|slotformulering|
    vraag|antwoord">
    <div class="{local-name()}">
      <xsl:apply-templates select="*" />
    </div>
  </xsl:template>

  <xsl:template
    match="artikel|artikeltekst">
    <div class="{local-name()}">
      <xsl:apply-templates select="kop" />
      <xsl:choose>
        <xsl:when test="lid">
          <xsl:variable name="start-type-style">
            <xsl:call-template name="list-start-type-style">
              <xsl:with-param name="nrs" select="lid/lidnr" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:choose>
            <xsl:when test="string($start-type-style) != ''">
              <xsl:variable
                name="start" select="exsl:node-set($start-type-style)/start" />
              <xsl:variable
                name="type" select="exsl:node-set($start-type-style)/type" />
              <xsl:variable
                name="style" select="exsl:node-set($start-type-style)/style" />
              <ol
                class="artikel_leden whitespace-small {$style}">
                <xsl:if test="$start != '1'">
                  <xsl:attribute name="start">
                    <xsl:value-of select="$start" />
                  </xsl:attribute>
                </xsl:if>
                <xsl:if test="$type != '1'">
                  <xsl:attribute name="type">
                    <xsl:value-of select="$type" />
                  </xsl:attribute>
                </xsl:if>
                <xsl:apply-templates select="lid" />
              </ol>
            </xsl:when>
            <xsl:otherwise>
              <ul class="expliciet artikel_leden whitespace-small">
                <xsl:apply-templates select="lid" mode="explicit" />
              </ul>
            </xsl:otherwise>
          </xsl:choose>
          
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="*[not(self::kop)]" />
        </xsl:otherwise>
      </xsl:choose>
    </div>
  </xsl:template>

  <xsl:template match="lid">
    <li>
      <xsl:apply-templates select="*[not(self::lidnr)]"/>
    </li>
  </xsl:template>

  <xsl:template match="lid/lidnr">
    <span class="lidnr ol">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="lidnr">
    <p class="lid">
      <span class="lidnr">
        <xsl:apply-templates />
      </span>
    </p>
  </xsl:template>

  <xsl:template match="definitielijst[@plaatsing='inline']">
    <dl class="inline">
      <xsl:apply-templates select="*" />
    </dl>
  </xsl:template>

  <xsl:template match="definitie-item">
    <dl>
      <xsl:apply-templates select="*" />
    </dl>
  </xsl:template>

  <xsl:template match="definitie-item/li.nr" />

  <xsl:template match="term[preceding-sibling::li.nr]">
    <dt>
      <span class="ol">
        <xsl:apply-templates select="preceding-sibling::li.nr/node()" />
      </span>
      <xsl:text> </xsl:text>
      <xsl:apply-templates />
    </dt>
  </xsl:template>

  <xsl:template match="term">
    <dt>
      <xsl:apply-templates />
    </dt>
  </xsl:template>

  <xsl:template match="definitie">
    <dd>
      <xsl:apply-templates select="*" />
    </dd>
  </xsl:template>

  <xsl:template match="wat|wij|considerans.al">
    <p class="{local-name()}">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="wijziging/nr" />

  <xsl:template match="wijziging/wat[preceding-sibling::nr]">
    <p class="wat labeled">
      <span class="nr">
        <xsl:apply-templates select="preceding-sibling::nr/node()" />
      </span>
      <xsl:text> </xsl:text>
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="al">
    <p>
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="afkondiging/al">
    <p class="afkondiging">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="nadruk[@type='vet']">
    <strong class="vet">
      <xsl:apply-templates />
    </strong>
  </xsl:template>

  <xsl:template match="nadruk[@type='cur']">
    <em class="cur">
      <xsl:apply-templates />
    </em>
  </xsl:template>

  <xsl:template match="extref[@soort='URL']">
    <a href="{@doc}">
      <xsl:apply-templates />
    </a>
  </xsl:template>
  
  <xsl:template match="extref[@soort='document']">
    <a href="/tkconv/op/{@doc}">
      <xsl:apply-templates />
    </a>
  </xsl:template>

  <xsl:template match="dossierref">
    <a href="/tkconv/ksd.html?ksd={@dossier}">
      <xsl:apply-templates />
    </a>
  </xsl:template>

  <xsl:template match="divisie|kop|tekst-sluiting|considerans|afkondiging">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="divisie/kop">
    <h2 class="divisiekop1">
      <xsl:apply-templates select="*" />
    </h2>
  </xsl:template>

  <xsl:template match="wijzig-artikel/kop">
    <h2 class="wijzig-artikel_kop">
      <xsl:apply-templates select="*" />
    </h2>
  </xsl:template>

  <xsl:template match="wijzig-divisie/kop">
    <h3 class="wijzig-divisie_kop">
      <xsl:apply-templates select="*" />
    </h3>
  </xsl:template>
  
  <xsl:template match="artikel/kop">
    <h3 class="artikel_kop no-toc">
      <xsl:apply-templates select="*" />
    </h3>
  </xsl:template>
  
  <xsl:template match="wijzig-divisie/artikel/kop" priority="1">
    <h4 class="artikel_kop no-toc">
      <xsl:apply-templates select="*" />
    </h4>
  </xsl:template>

  <xsl:template match="kop/*">
    <xsl:apply-templates />
    <xsl:if test="position() != last()">
      <xsl:text> </xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="al-groep">
    <div class="alineagroep">
      <xsl:apply-templates />
    </div>
  </xsl:template>

  <xsl:template match="lijst[@type='expliciet']">
    <xsl:variable name="start-type-style">
      <xsl:call-template name="list-start-type-style">
        <xsl:with-param name="nrs" select="li/li.nr" />
      </xsl:call-template>
    </xsl:variable>
    <xsl:choose>
      <xsl:when test="string($start-type-style) != ''">
        <xsl:variable
          name="start" select="exsl:node-set($start-type-style)/start" />
        <xsl:variable
          name="type" select="exsl:node-set($start-type-style)/type" />
        <xsl:variable
          name="style" select="exsl:node-set($start-type-style)/style" />
        <ol
          class="whitespace-small {$style}">
          <xsl:if test="$start != '1'">
            <xsl:attribute name="start">
              <xsl:value-of select="$start" />
            </xsl:attribute>
          </xsl:if>
          <xsl:if test="$type != '1'">
            <xsl:attribute name="type">
              <xsl:value-of select="$type" />
            </xsl:attribute>
          </xsl:if>
          <xsl:apply-templates select="li" />
        </ol>
      </xsl:when>
      <xsl:otherwise>
        <ul class="expliciet whitespace-small">
          <xsl:apply-templates select="li" mode="explicit" />
        </ul>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="li">
    <li class="li">
      <xsl:apply-templates select="*[not(self::li.nr)]" />
    </li>
  </xsl:template>

  <xsl:template match="li" mode="explicit">
    <li class="li">
      <xsl:apply-templates select="*[not(self::li.nr)]" mode="explicit" />
    </li>
  </xsl:template>

  <xsl:template match="li[li.nr]/al[1]" mode="explicit" priority="1">
    <p class="labeled">
      <xsl:apply-templates select="preceding-sibling::li.nr" />
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="li[li.nr]/*" mode="explicit">
    <xsl:apply-templates select="self::*" />
  </xsl:template>

  <xsl:template match="lid" mode="explicit">
    <li>
      <xsl:apply-templates select="*[not(self::lidnr)]" mode="explicit" />
    </li>
  </xsl:template>
  
  <xsl:template match="lid[lidnr]/al[1]" mode="explicit" priority="1">
    <p class="lid labeled">
      <xsl:apply-templates select="preceding-sibling::lidnr" />
      <xsl:apply-templates />
    </p>
  </xsl:template>
  
  <xsl:template match="lid[lidnr]/*" mode="explicit">
    <xsl:apply-templates select="self::*" />
  </xsl:template>

  <xsl:template match="li.nr">
    <span class="ol">
      <xsl:apply-templates />
      <xsl:text> </xsl:text>
    </span>
  </xsl:template>

  <xsl:template match="ondertekening">
    <p class="ondertekening">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="naam|achternaam|functie|dossiernummer|datum">
    <span class="{local-name()}">
      <xsl:apply-templates />
    </span>
  </xsl:template>

  <xsl:template match="table">
    <table>
      <xsl:apply-templates />
    </table>
    <xsl:if
      test=".//noot[@type='tabel']">
      <div class="tabelnoten">
        <hr />
        <xsl:apply-templates select=".//noot[@type='tabel']" mode="voetnoten" />
      </div>
    </xsl:if>
  </xsl:template>

  <xsl:template match="tgroup">
    <colgroup>
      <xsl:apply-templates select="colspec" />
    </colgroup>
    <xsl:apply-templates
      select="thead|tbody" />
  </xsl:template>

  <xsl:template match="colspec">
    <col />
  </xsl:template>

  <xsl:template match="thead">
    <thead>
      <xsl:apply-templates />
    </thead>
  </xsl:template>

  <xsl:template match="tbody">
    <tbody>
      <xsl:apply-templates />
    </tbody>
  </xsl:template>

  <xsl:template match="row">
    <tr>
      <xsl:apply-templates />
    </tr>
  </xsl:template>

  <xsl:template match="entry">
    <td>
      <xsl:apply-templates />
    </td>
  </xsl:template>

  <xsl:template match="sup">
    <sup>
      <xsl:apply-templates />
    </sup>
  </xsl:template>

  <xsl:template match="inf">
    <sub>
      <xsl:apply-templates />
    </sub>
  </xsl:template>

  <xsl:template match="voorstel-sluiting/label">
    <p class="gegeven">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="vraag/nr|antwoord/nr">
    <h2 class="stuktitel">
      <xsl:apply-templates/>
    </h2>
  </xsl:template>
  
  <!-- Helps the author of this xslt to see what still needs to be done -->
  <xsl:template match="*">
    <xsl:message terminate="no">
      <xsl:text>Unhandled element </xsl:text>
      <xsl:value-of select="local-name()" />
    </xsl:message>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="*" mode="title"> TODO </xsl:template>

  <xsl:template name="list-start-type-style">
    <xsl:param name="index" select="1"/>
    <xsl:param name="nrs" />

    <xsl:variable name="first-converted">
      <xsl:call-template name="one-list-start-type-style">
        <xsl:with-param name="nr" select="$nrs[$index]" />
      </xsl:call-template>
    </xsl:variable>
    <xsl:choose>
      <xsl:when test="$nrs[1+$index]">
        <xsl:variable name="next-converted">
          <xsl:call-template name="list-start-type-style">
            <xsl:with-param name="nrs" select="$nrs" />
            <xsl:with-param name="index" select="1+$index"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:if test="exsl:node-set($next-converted)/start = 1 + exsl:node-set($first-converted)/start">
          <xsl:copy-of select="exsl:node-set($first-converted)"/>
        </xsl:if>
      </xsl:when>
      <xsl:otherwise>
        <xsl:copy-of select="exsl:node-set($first-converted)"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="one-list-start-type-style">
    <xsl:param name="nr" />
    <xsl:variable name="res"
      select="smop:tokenize($nr, '.')" />
    <xsl:choose>
      <xsl:when test="count(smop:tokenize($res, '0123456789'))=0">
        <start>
          <xsl:value-of select="$res" />
        </start>
        <type>1</type>
        <style>list-style-dot</style>
      </xsl:when>
      <xsl:when test="count(smop:tokenize($res, 'abcdefghijklmnopqrstuvwxyz'))=0">
        <xsl:if test="string-length($res) = 1">
          <start>
            <xsl:value-of
              select="1+string-length(substring-before('abcdefghijklmnopqrstuvwxyz', $res))" />
          </start>
          <type>
            <xsl:text>a</xsl:text>
          </type>
          <style>
            <xsl:text>list-style-dot</xsl:text>
          </style>
        </xsl:if>
      </xsl:when>
      <xsl:when test="count(smop:tokenize($res, 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'))=0">
        <xsl:if test="string-length($res) = 1">
          <start>
            <xsl:value-of
              select="1+string-length(substring-before('ABCDEFGHIJKLMNOPQRSTUVWXYZ', $res))" />
          </start>
          <type>
            <xsl:text>A</xsl:text>
          </type>
          <style>
            <xsl:text>list-style-dot</xsl:text>
          </style>
        </xsl:if>
      </xsl:when>
    </xsl:choose>
  </xsl:template>
  
  <xsl:template name="voetnoten">
    <xsl:if
      test=".//noot[@type='voet']">
      <div id="noten">
        <hr />
        <xsl:apply-templates select=".//noot[@type='voet']" mode="voetnoten" />
      </div>
    </xsl:if>
  </xsl:template>

  <func:function name="smop:tokenize">
    <xsl:param name="string"/>
    <xsl:param name="delimiters"/>
    <xsl:choose>
      <xsl:when test="function-available('str:tokenize')">
        <func:result select="str:tokenize($string, $delimiters)"/>
      </xsl:when>
      <xsl:when test="function-available('saxon:tokenize')">
        <func:result select="saxon:tokenize($string, $delimiters)"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:message terminate="yes">No tokenize function available</xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </func:function>
</xsl:stylesheet>