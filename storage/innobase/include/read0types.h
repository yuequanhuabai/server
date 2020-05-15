/*****************************************************************************

Copyright (c) 1997, 2016, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2018, MariaDB Corporation.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/read0types.h
Cursor read

Created 2/16/1997 Heikki Tuuri
*******************************************************/

#ifndef read0types_h
#define read0types_h

#include "dict0mem.h"
#include "trx0types.h"
#include <algorithm>


/**
  Read view lists the trx ids of those transactions for which a consistent read
  should not see the modifications to the database.
*/
class ReadView
{
  /**
    View state.

    Implemented as atomic to allow mutex-free view close and re-use.
    Non-owner thread is allowed to call is_open() alone without mutex
    protection as well. E.g. trx_sys.view_count() does this.

    If non-owner thread intends to access other members as well, both
    is_open() and other members accesses must be pretected by trx->mutex.
    E.g. trx_sys.clone_oldest_view().
  */
  std::atomic<bool> m_open;

public:
  ReadView(): m_open(false), m_low_limit_id(0) {}


  /**
    Copy state from another view.

    This method is used to find min(m_low_limit_no), min(m_low_limit_id) and
    all transaction ids below min(m_low_limit_id). These values effectively
    form oldest view.

    @param other    view to copy from
  */
  void copy(const ReadView &other)
  {
    ut_ad(&other != this);
    if (!other.is_open())
      return;

    if (m_low_limit_no > other.m_low_limit_no)
      m_low_limit_no= other.m_low_limit_no;
    if (m_low_limit_id > other.m_low_limit_id)
      m_low_limit_id= other.m_low_limit_id;

    trx_ids_t::iterator dst= m_ids.begin();
    for (trx_ids_t::const_iterator src= other.m_ids.begin();
         src != other.m_ids.end(); src++)
    {
      if (*src >= m_low_limit_id)
        break;
loop:
      if (dst == m_ids.end())
      {
        m_ids.push_back(*src);
        dst= m_ids.end();
        continue;
      }
      if (*dst < *src)
      {
        dst++;
        goto loop;
      }
      else if (*dst > *src)
        dst= m_ids.insert(dst, *src) + 1;
    }
    m_ids.erase(std::lower_bound(dst, m_ids.end(), m_low_limit_id),
                m_ids.end());

    m_up_limit_id= m_ids.empty() ? m_low_limit_id : m_ids.front();
    ut_ad(m_up_limit_id <= m_low_limit_id);
  }


  /**
    Opens a read view where exactly the transactions serialized before this
    point in time are seen in the view.

    View becomes visible to purge thread.

    @param[in,out] trx transaction
  */
  void open(trx_t *trx);


  /**
    Closes the view.

    View becomes not visible to purge thread. Intended to be called by ReadView
    owner thread.
  */
  void close() { m_open.store(false, std::memory_order_relaxed); }


  /** Returns true if view is open. */
  bool is_open() const { return m_open.load(std::memory_order_relaxed); }


  /**
    Creates a snapshot where exactly the transactions serialized before this
    point in time are seen in the view.

    @param[in,out] trx transaction
  */
  inline void snapshot(trx_t *trx);


  /**
    Sets the creator transaction id.

    This should be set only for views created by RW transactions.
  */
  void set_creator_trx_id(trx_id_t id)
  {
    ut_ad(id > 0);
    ut_ad(m_creator_trx_id == 0);
    m_creator_trx_id= id;
  }


  /**
    Writes the limits to the file.
    @param file file to write to
  */
  void print_limits(FILE *file) const
  {
    if (is_open())
      fprintf(file, "Trx read view will not see trx with"
                    " id >= " TRX_ID_FMT ", sees < " TRX_ID_FMT "\n",
                    m_low_limit_id, m_up_limit_id);
  }


	/** Check whether transaction id is valid.
	@param[in]	id		transaction id to check
	@param[in]	name		table name */
	static void check_trx_id_sanity(
		trx_id_t		id,
		const table_name_t&	name);

	/** Check whether the changes by id are visible.
	@param[in]	id	transaction id to check against the view
	@param[in]	name	table name
	@return whether the view sees the modifications of id. */
	bool changes_visible(
		trx_id_t		id,
		const table_name_t&	name) const
		MY_ATTRIBUTE((warn_unused_result))
	{
		if (id < m_up_limit_id || id == m_creator_trx_id) {

			return(true);
		}

		check_trx_id_sanity(id, name);

		if (id >= m_low_limit_id) {

			return(false);

		} else if (m_ids.empty()) {

			return(true);
		}

		return(!std::binary_search(m_ids.begin(), m_ids.end(), id));
	}

	/**
	@param id		transaction to check
	@return true if view sees transaction id */
	bool sees(trx_id_t id) const
	{
		return(id < m_up_limit_id);
	}

	/**
	@return the low limit no */
	trx_id_t low_limit_no() const
	{
		return(m_low_limit_no);
	}

	/**
	@return the low limit id */
	trx_id_t low_limit_id() const
	{
		return(m_low_limit_id);
	}


private:
	/** The read should not see any transaction with trx id >= this
	value. In other words, this is the "high water mark". */
	trx_id_t	m_low_limit_id;

	/** The read should see all trx ids which are strictly
	smaller (<) than this value.  In other words, this is the
	low water mark". */
	trx_id_t	m_up_limit_id;

	/** trx id of creating transaction, set to TRX_ID_MAX for free
	views. */
	trx_id_t	m_creator_trx_id;

	/** Set of RW transactions that was active when this snapshot
	was taken */
	trx_ids_t	m_ids;

	/** The view does not need to see the undo logs for transactions
	whose transaction number is strictly smaller (<) than this value:
	they can be removed in purge if not needed by other views */
	trx_id_t	m_low_limit_no;
};

#endif
