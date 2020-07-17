#ifndef _XTADMA_TSN_
#define _XTADMA_TSN_

/* upper&lower stream fetch memory offsets */
#define XTADMA_USFM_OFFSET	0x1000
#define XTADMA_LSFM_OFFSET	0x2000

/* pointers memory offset */
#define XTADMA_PM_OFFSET	0x3000
#define XTADMA_PM_RD_MASK	0xFF
#define XTADMA_PM_WR_MASK	0xFF0000
#define XTADMA_PM_WR_SHIFT	16

/* Address length memory offset */
#define XTADMA_ALM_OFFSET	0x40000

#define XTADMA_CR_OFFSET		0x0
#define XTADMA_TO_OFFSET		0x4
#define XTADMA_FF_THRE_OFFSET		0x8
#define XTADMA_STR_ID_OFFSET		0xC
#define XTADMA_INT_EN_OFFSET		0x10
#define XTADMA_INT_STA_OFFSET		0x14
#define XTADMA_INT_CLR_OFFSET		0x18
#define XTADMA_EDI_FFI_STAT_OFFSET	0x20
#define XTADMA_NRDFI_FNDI_STAT_OFFSET	0x24
#define XTADMA_BEI_STNSI_STAT_OFFSET	0x28
#define XTADMA_BENSI_RESNSI_STAT_OFFSET	0x2C
#define XTADMA_SEI_DEI_STAT_OFFSET	0x30
#define XTADMA_IEI_STAT_OFFSET		0x34

#define XTADMA_FLIP_FETCH_MEM_NEW_SCHED     BIT(7)
#define XTADMA_FLIP_FETCH_MEM_NEXT_CYCLE	BIT(6)
#define XTADMA_HALTED		BIT(5)
#define XTADMA_SCHED_ENABLE	BIT(4)
#define XTADMA_SKIP_DEL_ENTRY	BIT(2)
#define XTADMA_SOFT_RST		BIT(1)
#define XTADMA_CFG_DONE		BIT(0)

#define XTADMA_OFFSET_TIME_SHIFT	16
#define XTADMA_OFFSET_TIME_MASK		(0xFFFF)

#define XTADMA_ENT_NUM_SEC_INTR_SHIFT	16
#define XTADMA_ENT_NUM_SEC_INTR_MASK	(0xFF)
#define XTADMA_FRAME_THRES_SHIFT	8
#define XTADMA_FRAME_THRES_MASK		(0xFF)

#define XTADMA_FIX_RES_QUEUE_ID_SHIFT	16
#define XTADMA_FIX_RES_QUEUE_ID_MASK	(0xFF0000)
#define XTADMA_FIX_BE_QUEUE_ID_SHIFT	0
#define XTADMA_FIX_BE_QUEUE_ID_MASK	(0xFF)

#define XTADMA_SEC_COMP_INT_EN		BIT(12)
#define XTADMA_IE_INT_EN		BIT(11)
#define XTADMA_SEI_INT_EN		BIT(10)
#define XTADMA_DEI_INT_EN		BIT(9)
#define XTADMA_BENSI_INT_EN		BIT(8)
#define XTADMA_RESNSI_INT_EN		BIT(7)
#define XTADMA_STNSI_INT_EN		BIT(6)
#define XTADMA_BEI_INT_EN		BIT(5)
#define XTADMA_NRDFI_INT_EN		BIT(4)
#define XTADMA_FNDI_INT_EN		BIT(3)
#define XTADMA_CDI_INT_EN		BIT(2)
#define XTADMA_EDI_INT_EN		BIT(1)
#define XTADMA_FFI_INT_EN		BIT(0)
#define XTADMA_INT_EN_ALL_MASK		(0x1FFF)

#define XTADMA_STR_FETCH_ENTRY_SIZE	64
#define XTADMA_STR_TIME_TICKS_SHIFT	0
#define XTADMA_STR_TIME_TICKS_MASK	(0x7FFFFFF)

#define XTADMA_STR_ID_SHIFT		0
#define XTADMA_STR_ID_MASK		0xFF
#define XTADMA_STR_NUM_FRM_SHIFT	16
#define XTADMA_STR_NUM_FRM_MASK		0x30000
#define XTADMA_STR_QUE_TYPE_SHIFT	20
#define XTADMA_STR_QUE_TYPE_MASK	0x300000
#define XTADMA_STR_CONT_FETCH_EN	BIT(22)
#define XTADMA_STR_ENTRY_VALID		BIT(31)

#define XTADMA_ALM_ADDR_MSB_SHIFT	0
#define XTADMA_ALM_ADDR_MSB_MASK	0xFF
#define XTADMA_ALM_TOT_PKT_SZ_BY8_SHIFT	8
#define XTADMA_ALM_TOT_PKT_SZ_BY8_MASK	0xFF00
#define XTADMA_ALM_FETCH_SZ_SHIFT	16
#define XTADMA_ALM_FETCH_SZ_MASK	0xFFF0000
#define XTADMA_ALM_UFF			BIT(28)
#define XTADMA_ALM_SOP			BIT(30)
#define XTADMA_ALM_EOP			BIT(31)

#define SFM_UPPER 0
#define SFM_LOWER 1

#define ST_PCP_VALUE 0x4

enum qtype {
	qt_st = 0,
	qt_res,
	qt_be,
	qt_resbe
};

/* address/length memory entry */
struct alm_entry {
	u32 addr;
	u32 cfg;
};

/* stream fetch entry */
struct sfm_entry {
	u32 tticks;
	u32 cfg;
};

struct tadma_cb {
	struct hlist_head *stream_hash;
	int streams;
	u32 be_trigger;
};

/* stream cfg */
struct tadma_stream {
	/*u8 is_trdp;
	u8 dmac[6];	
	short vid;*/
	u8 ip[4];
	u32 comid;
	u32 trigger;
	u32 count;
	u8 start;
};

/* stream entry */
struct tadma_stream_entry {
	u8 hashtag[8];
	u32 tticks;
	struct hlist_node hash_link;
	int sid;
	int sfm;
	int count;
};

/* trdp private header */
struct trdp_pd_hdr
{
    u32  sequenceCounter;                    /**< Unique counter (autom incremented)                     */
    u16  protocolVersion;                    /**< fix value for compatibility (set by the API)           */
    u16  msgType;                            /**< of datagram: PD Request (0x5072) or PD_MSG (0x5064)    */
    u32  comId;                              /**< set by user: unique id                                 */
    u32  etbTopoCnt;                         /**< set by user: ETB to use, '0' for consist local traffic */
    u32  opTrnTopoCnt;                       /**< set by user: direction/side critical, '0' if ignored   */
    u32  datasetLength;                      /**< length of the data to transmit 0...1432                */
    u32  reserved;                           /**< before used for ladder support                         */
    u32  replyComId;                         /**< used in PD request                                     */
    u32  replyIpAddress;                     /**< used for PD request                                    */
    u32  frameCheckSum;                      /**< CRC32 of header                                        */
};

static inline u32 tadma_ior(struct axienet_local *lp, off_t offset)
{
	return in_be32(lp->tadma_regs + offset);
}

static inline void tadma_iow(struct axienet_local *lp, off_t offset,
			     u32 value)
{
	out_be32((lp->tadma_regs + offset), value);
}
#endif /* _XTADMA_TSN_ */
